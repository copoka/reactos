#!/bin/bash
# Local wrapper: run the QEMU input harness on the build host and pull the
# evidence back into docs/test-logs/.
#
#   input-smoke.sh <app> <tag> --mode setup|desktop|hotplug [--usb] [--timeout N] [--iso NAME]
#
# <app> is a feature id (e.g. 0004-input-ps2-usb-hid) or "main". The ISO is
# looked up in <remote-tree>/output-MinGW-i386/bootcd.iso unless --iso is given
# (a bare name is resolved against the same directory, an absolute path is used
# as-is). Evidence lands next to this script:
#   docs/test-logs/<date>-<tag>/{serial.log,serial-*.log,shot-*.png,qemu.out,harness.log}
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)                 # .../features/<id>/fork
FEATURE=$(dirname "$REPO")                      # .../features/<id>
ROOT=$(cd "$FEATURE/../.." && pwd)              # reactos-lab root
ENV_FILE=${ROS_ENV_FILE:-$ROOT/.env}
if [[ ! -f $ENV_FILE ]]; then
  # fall back to walking up from here (script copied outside the lab tree)
  d=$HERE
  while [[ $d != / && ! -f $d/.env ]]; do d=$(dirname "$d"); done
  ENV_FILE=$d/.env
fi
set -a; . "$ENV_FILE"; set +a

APP=${1:?usage: input-smoke.sh <app> <tag> --mode ...}
TAG=${2:?tag}
shift 2
MODE=setup; USB=0; TIMEOUT=180; ISO_NAME=bootcd.iso
while [ $# -gt 0 ]; do
  case $1 in
    --mode) MODE=$2; shift 2;;
    --usb) USB=1; shift;;
    --timeout) TIMEOUT=$2; shift 2;;
    --iso) ISO_NAME=$2; shift 2;;
    *) echo "unknown arg: $1" >&2; exit 2;;
  esac
done

HOST=${ROS_REMOTE_HOST:?}
USER_NAME=${ROS_REMOTE_USER:?}
PORT=${ROS_REMOTE_PORT:-22}
RROOT=${ROS_REMOTE_ROOT:?}
KEY=${ROS_REMOTE_KEY:-}
PASSWORD=${ROS_REMOTE_PASSWORD:-}
SSH_OPTS=(-p "$PORT" -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=/dev/null
          -o LogLevel=ERROR -o ServerAliveInterval=30 -o ServerAliveCountMax=6)
if [[ -n $KEY && -r $KEY ]]; then
  AUTH_PREFIX=(); SSH_AUTH=(-i "$KEY" -o IdentitiesOnly=yes -o BatchMode=yes)
else
  : "${PASSWORD:?нет ключа и ROS_REMOTE_PASSWORD}"
  export SSHPASS="$PASSWORD"
  AUTH_PREFIX=(sshpass -e); SSH_AUTH=()
fi
SSH=("${AUTH_PREFIX[@]}" ssh "${SSH_OPTS[@]}" "${SSH_AUTH[@]}")
SCP=("${AUTH_PREFIX[@]}" scp -P "$PORT" "${SSH_AUTH[@]}"
     -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR)

RAPP="$RROOT/$APP"
RDIR="$RROOT/.qemu-input/$APP/$TAG"
case $ISO_NAME in
  /*) RISO=$ISO_NAME;;
  *)  RISO="$RAPP/output-MinGW-i386/$ISO_NAME";;
esac
DATE=$(date +%F)
LOCAL="$REPO/docs/test-logs/$DATE-$TAG"
HOST_SCRIPT="$HERE/qemu-input-host.sh"

[[ -f $HOST_SCRIPT ]] || { echo "missing $HOST_SCRIPT" >&2; exit 2; }
rm -rf "$LOCAL"
mkdir -p "$LOCAL"

echo "== harness: app=$APP tag=$TAG mode=$MODE usb=$USB timeout=$TIMEOUT"
echo "== remote: $USER_NAME@$HOST:$RDIR iso=$RISO"

"${SSH[@]}" "$USER_NAME@$HOST" "mkdir -p '$RDIR' && test -r '$RISO'" \
  || { echo "FAIL: ISO not readable on host: $RISO" >&2; exit 3; }
"${SCP[@]}" "$HOST_SCRIPT" "$USER_NAME@$HOST:$RDIR/qemu-input-host.sh"

USB_ARG=()
[ "$USB" = 1 ] && USB_ARG=(--usb)
set +e
"${SSH[@]}" "$USER_NAME@$HOST" \
  "chmod +x '$RDIR/qemu-input-host.sh' && cd '$RDIR' && ./qemu-input-host.sh '$RDIR' '$RISO' '$MODE' '$TIMEOUT' ${USB_ARG[*]:-}" \
  | tee "$LOCAL/harness.log"
RC=${PIPESTATUS[0]}
set -e

"${SCP[@]}" "$USER_NAME@$HOST:$RDIR/serial.log" "$LOCAL/serial.log" 2>/dev/null || true
"${SCP[@]}" "$USER_NAME@$HOST:$RDIR/serial-*.log" "$LOCAL/" 2>/dev/null || true
"${SCP[@]}" "$USER_NAME@$HOST:$RDIR/qemu.out" "$LOCAL/qemu.out" 2>/dev/null || true
"${SCP[@]}" "$USER_NAME@$HOST:$RDIR/shot-*.ppm" "$LOCAL/" 2>/dev/null || true

# PPM -> PNG locally (PIL); drop the raw PPM once converted.
python3 - "$LOCAL" <<'EOF'
import glob, os, sys
try:
    from PIL import Image
except Exception:
    print('convert: PIL missing, keeping PPM')
    sys.exit(0)
d = sys.argv[1]
for p in sorted(glob.glob(os.path.join(d, 'shot-*.ppm'))):
    try:
        Image.open(p).save(p[:-4] + '.png')
        os.remove(p)
    except Exception as e:
        print('convert failed %s: %s' % (p, e))
EOF

echo "== rc=$RC"
echo "== evidence: $LOCAL"
ls -la "$LOCAL"
exit "$RC"
