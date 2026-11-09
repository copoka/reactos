#!/bin/bash
# Host-side QEMU input driver (runs on the build host, not locally).
#   qemu-input-host.sh <dir> <iso> <mode:setup|desktop|hotplug> <timeout> [--usb]
# Modes:
#   setup   - wait for the text-mode Setup screen, send Down/Up/Enter, dump
#             frames before/after and report the pixel difference.
#   desktop - wait for a stable graphical screen, Win+R -> "notepad" -> Enter,
#             type "abc", then mouse move / click / drag; dump frames.
#   hotplug - boot with --usb (usb-kbd), at t=60s device_add usb-mouse, observe
#             another 60s, dump frames and a serial snapshot of the hot-plug
#             window.
# Writes: serial.log, serial-*.log, shot-*.ppm, qemu.out, qemu.pid.
# Prints "VERDICT: ..." and exits non-zero on a bugcheck.
set -u
DIR=${1:?dir}; ISO=${2:?iso}; MODE=${3:-setup}; TIMEOUT=${4:-180}
shift 4 2>/dev/null || shift $#
USB=0
for a in "$@"; do case $a in --usb) USB=1;; esac; done

mkdir -p "$DIR" && cd "$DIR" || exit 2
rm -f serial.log serial-*.log shot-*.ppm qemu.out qemu.pid

QEMU_ARGS=(
  -enable-kvm -cpu qemu32 -m 1024 -smp 1
  -cdrom "$ISO" -boot d
  -display none -vga std
  -serial file:serial.log
  -monitor unix:mon.sock,server,nowait
  -no-reboot -net none -rtc base=localtime
)
if [ "$USB" = 1 ]; then
  QEMU_ARGS+=(-usb -device usb-kbd -device usb-mouse)
fi

setsid qemu-system-i386 "${QEMU_ARGS[@]}" > qemu.out 2>&1 &
QPID=$!
echo "$QPID" > qemu.pid
sleep 2
if ! kill -0 "$QPID" 2>/dev/null; then
  echo "FAIL: qemu died at start"
  cat qemu.out
  exit 2
fi

python3 - "$DIR" "$MODE" "$TIMEOUT" <<'EOF'
import os, re, shutil, socket, sys, time

d, mode, timeout = sys.argv[1], sys.argv[2], int(sys.argv[3])
mon = os.path.join(d, 'mon.sock')
serial = os.path.join(d, 'serial.log')
t0 = time.time()
events = []

def log(msg):
    print('[%6.1fs] %s' % (time.time() - t0, msg), flush=True)
    events.append(msg)

def read_ppm(path):
    with open(path, 'rb') as f:
        data = f.read()
    if not data.startswith(b'P6'):
        raise ValueError('not P6: ' + path)
    parts, pos = [], 2
    while len(parts) < 3:
        while pos < len(data) and data[pos:pos+1].isspace():
            pos += 1
        if data[pos:pos+1] == b'#':
            while data[pos:pos+1] not in (b'\n', b''):
                pos += 1
            continue
        s = pos
        while pos < len(data) and not data[pos:pos+1].isspace():
            pos += 1
        parts.append(int(data[s:pos]))
    pos += 1
    w, h, mx = parts
    return w, h, data[pos:pos + w * h * 3]

def stats(path):
    w, h, px = read_ppm(path)
    n = w * h
    nonblack = sum(1 for i in range(0, len(px), 3) if px[i] or px[i+1] or px[i+2])
    return w, h, nonblack / float(n or 1)

def diff(a, b):
    wa, ha, pa = read_ppm(a)
    wb, hb, pb = read_ppm(b)
    if (wa, ha) != (wb, hb) or len(pa) != len(pb):
        return None
    n = wa * ha
    changed = 0
    for i in range(0, len(pa), 3):
        if pa[i:i+3] != pb[i:i+3]:
            changed += 1
    return changed, n

class Mon:
    def __init__(self, path):
        self.path = path
    def cmd(self, c, wait=0.6):
        s = socket.socket(socket.AF_UNIX)
        s.settimeout(10)
        try:
            s.connect(self.path)
        except OSError as e:
            print('[dead] monitor connect failed: %s' % e, flush=True)
            s.close()
            return ''

        try:
            s.recv(65536)
            s.sendall((c + '\n').encode())
            time.sleep(wait)
            try:
                return s.recv(65536).decode(errors='replace')
            except Exception:
                return ''
        finally:
            s.close()
    def shot(self, name, wait=0.8):
        p = os.path.join(d, name)
        self.cmd('screendump %s' % p, wait)
        return p if os.path.exists(p) else None

for _ in range(120):
    if os.path.exists(mon):
        break
    time.sleep(0.5)
else:
    sys.exit(3)
m = Mon(mon)

def serial_size():
    try:
        return os.path.getsize(serial)
    except OSError:
        return 0

def serial_tail_since(off, limit=200):
    try:
        with open(serial, 'rb') as f:
            f.seek(off)
            data = f.read()
    except OSError:
        return ''
    lines = [l for l in data.decode('utf-8', 'replace').splitlines() if l.strip()]
    return '\n'.join(lines[-limit:])

def wait_until(seconds, pred, step=3.0):
    while time.time() - t0 < seconds:
        p = pred()
        if p:
            return p
        time.sleep(step)
    return None

def serial_text():
    try:
        with open(serial, 'rb') as f:
            return f.read().decode('utf-8', 'replace')
    except OSError:
        return ''

def serial_has(marker):
    return marker in serial_text()

def has_bugcheck():
    t = serial_text()
    return 'Fatal System Error' in t or '0xC000021A' in t

def wait_serial(marker, seconds, step=2.0):
    while time.time() - t0 < seconds:
        if serial_has(marker):
            log('serial marker found: %s' % marker)
            return True
        if has_bugcheck():
            log('serial shows a bugcheck while waiting for %s' % marker)
            return False
        time.sleep(step)
    return False

def text_setup_screen(deadline, min_frac=0.20):
    """ReactOS usetup is a 720x400 text screen with a filled (blue) background;
    FreeLoader is 720x400 but nearly black, the boot logo is 640x480."""
    while time.time() - t0 < deadline:
        p = m.shot('shot-probe.ppm', 0.8)
        if p:
            try:
                w, h, frac = stats(p)
            except Exception:
                time.sleep(2); continue
            log('probe %dx%d nonblack=%.3f' % (w, h, frac))
            if h == 400 and frac > min_frac:
                time.sleep(2)
                q = m.shot('shot-probe2.ppm', 0.8)
                if q:
                    dd = diff(p, q)
                    if dd and dd[0] <= 0.002 * dd[1]:
                        return q
        if has_bugcheck():
            log('bugcheck before the Setup screen')
            return None
        time.sleep(2)
    return None

verdict = []
if mode == 'setup':
    # FreeLoader shows the boot menu first (720x400, black background). The
    # default entry is "Live (Debug)"; the PS/2 test needs the native text-mode
    # usetup with COM1 debug, i.e. "Setup (Debug)" (two lines above the
    # default). Move there and press Enter before the 9 s auto-boot timeout.
    while time.time() - t0 < 2.0:
        time.sleep(0.2)
    m.shot('shot-freeloader-menu.ppm', 1.0)
    for _ in range(2):
        log('sendkey up (boot menu: Live (Debug) -> Setup (Debug))')
        m.cmd('sendkey up', 0.35)
    m.shot('shot-freeloader-setup-selected.ppm', 0.8)
    log('sendkey ret (boot Setup (Debug), text mode)')
    m.cmd('sendkey ret', 1.0)
    ready = text_setup_screen(min(timeout - 25, 120), min_frac=0.5)
    if not ready:
        log('text-mode Setup screen not detected')
        verdict.append('setup-screen-not-detected')
        m.shot('shot-timeout.ppm', 1.0)
    else:
        verdict.append('setup-text-screen')
        before = m.shot('shot-keys-before.ppm', 1.0)
        log('sendkey down')
        m.cmd('sendkey down', 1.5)
        mid = m.shot('shot-keys-mid.ppm', 1.2)
        log('sendkey up')
        m.cmd('sendkey up', 1.5)
        restore = m.shot('shot-keys-restored.ppm', 1.2)
        log('sendkey ret')
        m.cmd('sendkey ret', 4.0)
        after = m.shot('shot-keys-after.ppm', 1.5)
        for label, x, y in (('down', before, mid), ('up', mid, restore), ('ret', restore, after)):
            if not (x and y):
                verdict.append('%s-no-frame' % label)
                continue
            dd = diff(x, y)
            if not dd:
                verdict.append('%s-frame-size-mismatch' % label)
                continue
            log('pixel diff %s: %d/%d (%.3f%%)' % (label, dd[0], dd[1], 100.0 * dd[0] / dd[1]))
            verdict.append('key-%s-changed' % label if dd[0] > 100 else 'key-%s-no-change' % label)
elif mode == 'desktop':
    # The desktop needs the kernel and win32k; without the 0001 test vehicle the
    # LiveCD is expected to bugcheck (0xC000021A) and we stop early.
    saw_win32k = wait_serial('win32k.sys', min(timeout - 60, 90))
    if has_bugcheck():
        log('bugcheck before the desktop (expected without 0001)')
        verdict.append('bugcheck-before-desktop')
        m.shot('shot-bugcheck.ppm', 1.0)
        ready = None
    else:
        ready = None
        deadline = max(60, timeout - 60)
        while time.time() - t0 < deadline:
            p = m.shot('shot-probe.ppm', 0.8)
            if p:
                try:
                    w, h, frac = stats(p)
                except Exception:
                    time.sleep(3); continue
                log('probe %dx%d nonblack=%.3f' % (w, h, frac))
                if h >= 480 and frac > 0.03:
                    q = m.shot('shot-probe2.ppm', 0.8)
                    if q:
                        dd = diff(p, q)
                        if dd and dd[0] <= 0.01 * dd[1]:
                            ready = q
                            break
            if has_bugcheck():
                log('bugcheck while waiting for the desktop')
                verdict.append('bugcheck-before-desktop')
                break
            time.sleep(3)
    if not ready:
        log('stable desktop screen not detected (win32k marker=%s)' % saw_win32k)
        verdict.append('desktop-not-detected')
        m.shot('shot-desktop-timeout.ppm', 1.0)
    else:
        m.shot('shot-desktop.ppm', 1.0)
        try:
            dw, dh, _ = stats(ready)
        except Exception:
            dw, dh = 800, 600
        cur = [dw // 2, dh // 2]

        def move_to(x, y):
            """Relative mouse_move in small steps, tracking the cursor."""
            x = max(0, min(dw - 1, x)); y = max(0, min(dh - 1, y))
            while (cur[0], cur[1]) != (x, y):
                dx = max(-60, min(60, x - cur[0]))
                dy = max(-60, min(60, y - cur[1]))
                m.cmd('mouse_move %d %d' % (dx, dy), 0.15)
                cur[0] += dx; cur[1] += dy
            time.sleep(0.2)

        def click():
            # QEMU PS/2 drops very short clicks: hold the button a bit longer.
            m.cmd('mouse_button 1', 0.45)
            m.cmd('mouse_button 0', 0.45)
            time.sleep(0.2)

        # The LiveCD launcher window sits at (160,130)-(640,465) on 800x600.
        # Grab its title bar and drag it: this is the mouse-drag evidence and it
        # works before any shell state exists.
        move_to(dw // 2, 135)
        pre_drag = m.shot('shot-drag-before.ppm', 0.8)
        m.cmd('mouse_button 1', 0.3)
        for i in range(3):
            m.cmd('mouse_move 20 13', 0.2)
            cur[0] += 20; cur[1] += 13
        m.cmd('mouse_button 0', 0.5)
        drag = m.shot('shot-window-drag.ppm', 1.2)
        if pre_drag and drag:
            dd = diff(pre_drag, drag)
            if dd:
                log('pixel diff after window drag: %d/%d (%.3f%%)' % (dd[0], dd[1], 100.0 * dd[0] / dd[1]))
                verdict.append('drag-moved-window' if dd[0] > 2000 else 'drag-no-window-move')
        # Page 1 "Next", page 2 "Run ReactOS Live CD": both moved with the window.
        move_to(506 + 60, 442 + 39)
        click()
        time.sleep(2)
        m.shot('shot-launcher-page2.ppm', 1.0)
        # The wizard re-centres itself when it switches to page 2.
        move_to(400, 344)  # Run ReactOS Live CD

        click()
        time.sleep(8)
        m.shot('shot-desktop-ready.ppm', 1.0)
        move_to(60, dh - 60)
        click()
        time.sleep(0.5)
        log('open Run dialog (meta_l-r)')
        m.cmd('sendkey meta_l-r', 2.0)
        pre = m.shot('shot-run-dialog.ppm', 1.0)
        for k in ('n', 'o', 't', 'e', 'p', 'a', 'd'):
            m.cmd('sendkey %s' % k, 0.25)
        m.cmd('sendkey ret', 5.0)
        pre_text = m.shot('shot-notepad-open.ppm', 1.5)
        for k in ('a', 'b', 'c'):
            m.cmd('sendkey %s' % k, 0.3)
        text = m.shot('shot-notepad-abc.ppm', 1.5)
        if pre_text and text:
            dd = diff(pre_text, text)
            if dd:
                log('pixel diff after typing abc: %d/%d (%.3f%%)' % (dd[0], dd[1], 100.0 * dd[0] / dd[1]))
                verdict.append('typing-changed-screen' if dd[0] > 20 else 'typing-no-change')
            else:
                verdict.append('typing-frame-size-mismatch')
        # Mouse click: open the Start menu with a real click and dump it.
        move_to(20, dh - 15)
        pre_click = m.shot('shot-click-before.ppm', 0.8)
        click()
        menu = m.shot('shot-start-menu.ppm', 1.2)
        if pre_click and menu:
            dd = diff(pre_click, menu)
            if dd:
                log('pixel diff after Start click: %d/%d (%.3f%%)' % (dd[0], dd[1], 100.0 * dd[0] / dd[1]))
                verdict.append('mouse-click-opened-menu' if dd[0] > 2000 else 'mouse-click-no-menu')
        m.cmd('sendkey esc', 0.8)
        # Mouse drag: grab the Notepad title bar and move the window.
        move_to(300, 82)
        pre_drag2 = m.shot('shot-notepad-drag-before.ppm', 0.8)
        m.cmd('mouse_button 1', 0.4)
        for i in range(5):
            m.cmd('mouse_move -20 10', 0.2)
            cur[0] -= 20; cur[1] += 10
        m.cmd('mouse_button 0', 0.6)
        drag2 = m.shot('shot-notepad-dragged.ppm', 1.2)
        if pre_drag2 and drag2:
            dd = diff(pre_drag2, drag2)
            if dd:
                log('pixel diff after Notepad drag: %d/%d (%.3f%%)' % (dd[0], dd[1], 100.0 * dd[0] / dd[1]))
                verdict.append('mouse-drag-moved-notepad' if dd[0] > 2000 else 'mouse-drag-no-move')
        # Click into the editor, then type one more char: proves the click placed
        # the caret (text lands after the click target).
        move_to(400, 200)
        click()
        m.cmd('sendkey d', 0.4)
        m.shot('shot-notepad-after-click.ppm', 1.2)
    m.shot('shot-desktop-final.ppm', 1.0)
elif mode == 'hotplug':
    # Static usb-kbd/usb-mouse (with --usb) plus a runtime device_add usb-mouse
    # at t=60s. Only enumeration/binding is checked here; functional USB input
    # needs the desktop (pending 0001).
    wait_serial('i8042prt.sys', min(timeout - 60, 90))
    while time.time() - t0 < 60:
        time.sleep(1)
    m.shot('shot-045.ppm', 1.0)
    off = serial_size()
    log('device_add usb-mouse (serial offset %d)' % off)
    m.cmd('device_add usb-mouse', 1.0)
    time.sleep(10)
    try:
        shutil.copyfile(serial, os.path.join(d, 'serial-hotplug-window.log'))
    except Exception as e:
        log('serial snapshot failed: %s' % e)
    hot = serial_tail_since(off, 400)
    hot_lines = hot.splitlines()
    log('serial lines after hot-plug: %d' % len(hot_lines))
    for l in hot_lines[-12:]:
        print('    ' + l, flush=True)
    verdict.append('hotplug-serial-activity' if len(hot_lines) > 5 else 'hotplug-serial-quiet')
    m.shot('shot-075.ppm', 1.0)
    while time.time() - t0 < min(timeout, 125):
        time.sleep(2)
    m.shot('shot-hotplug-final.ppm', 1.0)
else:
    print('FAIL: unknown mode %s' % mode)
    sys.exit(2)

log('info status: ' + m.cmd('info status', 0.5).strip().replace('\n', ' | '))
m.cmd('quit', 0.5)
print('VERDICT: ' + (','.join(verdict) if verdict else 'no-evidence'), flush=True)
EOF

for _ in $(seq 1 40); do kill -0 "$QPID" 2>/dev/null || break; sleep 0.5; done
kill -0 "$QPID" 2>/dev/null && kill "$QPID" 2>/dev/null
sleep 1
kill -9 "$QPID" 2>/dev/null

echo "--- serial.log $(wc -c < serial.log) bytes"
grep -n "Fatal System Error" serial.log | head -3
if grep -q "Fatal System Error" serial.log; then
  echo "VERDICT: bugcheck"
  grep -m1 -A2 "Fatal System Error" serial.log
  exit 1
fi
echo "VERDICT: no-fatal-error"
echo "--- serial driver lines"
for pat in i8042prt kbdclass mouclass usbuhci usbhub hidusb kbdhid mouhid; do
  printf 'serial %-10s %s\n' "$pat" "$(grep -c "$pat" serial.log)"
done
exit 0
