# 0004-input-ps2-usb-hid — QEMU input evidence (2026-11-09)

итог: 0 errors в сборке, T1/T3/T4/T8/T10 — PASS; T2 — ожидаемый baseline
0xC000021A без 0001; T5/T6/T7/T11 проверены на test vehicle 0001 (в ветку не
входит) и остаются `pending 0001` для чистой ветки; T9 не закрыт (USB HID ввод
функционально не инжектился, только энумерация/биндинг).

## Стенд

| | |
|---|---|
| хост сборки/QEMU | `ai1` (12 cores, KVM), дерево `/home/l/build/reactos-lab/0004-input-ps2-usb-hid` |
| локальный хост | рабочая станция лаборатории, только подготовка/конвертация PNG |
| QEMU | `qemu-system-i386 -enable-kvm -cpu qemu32 -m 1024 -smp 1 -cdrom ISO -boot d -display none -vga std -monitor unix:...` |
| харнесс | `fork/docs/test-tools/qemu-input-host.sh` (host-side), `fork/docs/test-tools/input-smoke.sh` (localscp-обёртка) |
| режимы | `setup` (FreeLoader → Setup (Debug) → sendkey down/up/ret), `desktop` (Live → wizard → Run→notepad→abc, mouse click/drag), `hotplug` (`--usb`, `device_add usb-mouse` на 60-й секунде) |

## Команды и rc

```text
# T1 сборка (локально)
cd /home/l/projects/reactos-lab && scripts/remote-build.sh build 0004-input-ps2-usb-hid bootcd
rc=0   лог: docs/test-logs/2026-11-09-0004-build.log

# T4/T3 + USB в текстовом Setup (ISO A)
fork/docs/test-tools/input-smoke.sh 0004-input-ps2-usb-hid setup-ps2  --mode setup   --timeout 150
rc=0
fork/docs/test-tools/input-smoke.sh 0004-input-ps2-usb-hid setup-usb  --mode setup --usb --timeout 150
rc=0

# T2 baseline Live без 0001 (ISO A)
fork/docs/test-tools/input-smoke.sh 0004-input-ps2-usb-hid baseline-live --mode desktop --timeout 180
rc=1  (VERDICT: bugcheck 0xc000021a — ожидаемо)

# T5/T6/T7/T11 GUI (ISO B = HEAD + test vehicle 0001)
fork/docs/test-tools/input-smoke.sh 0004-input-ps2-usb-hid tv0001-desktop8 --mode desktop \
  --timeout 180 --iso bootcd-0001-testvehicle.iso
rc=0

# T8/T10 USB HID + hot-plug (ISO B)
fork/docs/test-tools/input-smoke.sh 0004-input-ps2-usb-hid tv0001-usb-hotplug --mode hotplug --usb \
  --timeout 150 --iso bootcd-0001-testvehicle.iso
rc=0
```

Test vehicle 0001 применён только в дереве на `ai1` (`patch -p1` + `touch
sdk/cmake/CMakeMacros.cmake` + `ninja -j 8 bootcd`), после патча `sync` не
делался. Подробности: `docs/test-logs/2026-11-09-0001-testvehicle-build.log`.

## ISO

| | ISO A (ветка) | ISO B (HEAD + test vehicle 0001) |
|---|---|---|
| path | `output-MinGW-i386/bootcd.iso` | `output-MinGW-i386/bootcd-0001-testvehicle.iso` |
| size | 373293056 (356 MiB) | 373293056 (356 MiB) |
| sha256 | `10e9492793c4b44dd4b0daf423b01efe99d15539676264401628907ffbf86061` | `83bd47a744c04cd55d8172b9f2f082060f793fbcef1ae8c080c60e4ed75dd306` |

ISO B в ветку не входит и в git не попадает; после применения патча
`bootcd.iso` на `ai1` тоже стал B.

## Матрица

| # | Сценарий | Итог | Доказательство |
|---|---|---|---|
| T1 | Сборка `bootcd` | **PASS** rc=0 | `2026-11-09-0004-build.log`, sha256 выше |
| T2 | Live без bugcheck | **baseline (ожидаемо FAIL без 0001)** `Fatal System Error: 0xc000021a` в smss (0xC000036F) | `2026-11-09-baseline-live/serial.log:140` |
| T3 | PS/2 драйверы в serial | **PASS**: `i8042prt.sys`, `kbdclass.sys`, `mouclass.sys` | `2026-11-09-baseline-live/serial.log:110,112,114` |
| T4 | PS/2 клавиатура в Setup Text Mode | **PASS**: Down/Up/Enter меняют экран, 6.744% / 3.043% / 8.614% пикселей | `2026-11-09-setup-ps2/shot-keys-*.png` |
| T5 | Desktop: Win+R → notepad → `abc` | **PASS на ISO B**, `pending 0001` для чистой ветки: в Notepad `abc`, Line 1 col 4 | `2026-11-09-tv0001-desktop8/shot-notepad-abc.png` |
| T6 | Мышь: move/click/drag | **PASS на ISO B**, `pending 0001` для чистой ветки: клик открыл Start (56037 px / 11.674%), перетащено окно Notepad (164080 px / 34.183%), перетащено окно LiveCD (142297 px / 29.645%) | `shot-start-menu.png`, `shot-notepad-dragged.png`, `shot-window-drag.png` |
| T7 | Нет залипших клавиш/дребезга | **PASS на ISO B**, `pending 0001` для чистой ветки: `sendkey a,b,c` → ровно 3 символа (колонка 4); после клика `d` — колонка 5 | `shot-notepad-abc.png`, `shot-notepad-after-click.png` |
| T8 | USB HID энумерация/биндинг | **PASS**: `hidusb.sys` + `hidclass.sys` + `kbdhid.sys` (+`mouhid.sys` в Live), `hidusb.c:1418 Status 0`, usbhub PnP `Installing: HID\Vid_0627&Pid_0001`, нет bugcheck. `usbuhci.sys` в serial не логируется (см. находки) | `2026-11-09-setup-usb/serial.log:151,152,159`; `2026-11-09-tv0001-usb-hotplug/serial.log:152,153,160,166,358-360` |
| T9 | USB HID ввод на desktop | **pending 0001 / не закрыт**: ввод через monitor идёт в PS/2-путь; отдельного USB-инжекта нет. Энумерация и биндинг подтверждены (T8) | — |
| T10 | Hot-plug USB-мыши | **PASS (с находкой)**: на 60-й секунде `device_add usb-mouse` → `USB\Vid_0627&Pid_0001\5&c3e83e4b&2`, `USBPORT_OpenPipe`, `USBH_PdoStartDevice`, `hidusb Status 0`, bugcheck нет; динамическая установка `DevInstallW failed with error 31` | `2026-11-09-tv0001-usb-hotplug/serial.log:493-508`, `serial-hotplug-window.log` |
| T11 | US-раскладка по умолчанию | **PASS на ISO B**, `pending 0001` для чистой ветки: `abc` → `abc` (US) | `shot-notepad-abc.png` |
| R1 | Setup доходит до выбора языка | **PASS**: "Language Selection", ENTER = Continue | `2026-11-09-setup-ps2/shot-keys-before.png` |
| R2 | Регрессии ввода/USB/загрузки | новых bugcheck нет; IRQ 1 conflict — известный; новые ERR см. находки | serial-логи прогонов |

## Ключевые строки serial

```text
# Setup (Debug), ISO A — PS/2 и USB HID
Loading: \SystemRoot\system32\drivers\i8042prt.sys at AF0FE000 with 1d pages
Resource conflict: IRQ (0x1 0x1 vs. 0x1 0x1)              # известный IRQ1 conflict
Loading: \SystemRoot\system32\drivers\hidusb.sys at AFFA9000 with d pages
Loading: \SystemRoot\system32\drivers\hidclass.sys at AF0EA000 with 14 pages
Loading: \SystemRoot\system32\drivers\kbdhid.sys at AFF9D000 with c pages
(/drivers/hid/hidusb/hidusb.c:1418) Status 0            # HID-отчёт USB-клавиатуры
# mouclass.sys/mouhid.sys в MININT отсутствуют: ZwOpenFile failed 0xc0000034

# Live (Debug), ISO A — baseline T2
Loading: ... i8042prt.sys / kbdclass.sys / mouclass.sys
*** Fatal System Error: 0xc000021a   (0x0031FDAC,0xC000036F,...)  # smss

# Live (Debug) + --usb, ISO B — T8/T10
Loading: ... kbdhid.sys / mouhid.sys / hidusb.sys / hidclass.sys
Installing: HID\Vid_0627&Pid_0001\5&c769bb1f&0001
USBH_PdoStartDevice: call IoWMIRegistrationControl UNIMPLEMENTED. FIXME
(/drivers/hid/hidusb/hidusb.c:1418) Status 0
# hot-plug на 60 c: device_add usb-mouse →
Installing: USB\Vid_0627&Pid_0001\5&c3e83e4b&2
err:(/dll/win32/newdev/newdev.c:1039) DevInstallW failed with error 31
InstallDevice failed for DeviceInstance 'USB\Vid_0627&Pid_0001\5&c3e83e4b&2'
```

## Находки

1. **FreeLoader: нужен правильный пункт меню.** Дефолт — `Live (Debug)`;
   без 0001 Live падает в `0xC000021A` до win32k. Текстовый Setup — это
   `Setup (Debug)` (2 строки выше дефолта, `Up`×2 + `Enter`), он стартует
   usetup (`MININT`) и не требует 0001. «Setup Text Mode» (верхний пункт) идёт
   без COM1-отладки — для доказательств берём `Setup (Debug)`.
2. **T3 в чистом Setup неполный по составу драйверов**: usetup грузит только
   `i8042prt.sys`; `mouclass.sys`/`mouhid.sys` в MININT-образе отсутствуют
   (`0xc0000034`), `kbdclass.sys` не грузится (usetup работает со скан-кодами
   i8042prt напрямую). Полный набор PS/2-драйверов подтверждён на Live
   (baseline-прогон).
3. **`usbuhci.sys` не появляется в serial** ни в одном прогоне (0 вхождений),
   хотя UHCI-контроллер работает: usbhub перечисляет устройства, HID
   устанавливается. Вероятно, загрузка минипорта не логируется — это не
   дефект фичи, но критерий T8 «usbuhci в serial» формально не выполним в этом
   build; энумерация доказана строками usbhub/hidusb.
4. **Hot-plug: ошибка установки 31.** Новое устройство энумерируется и
   стартует (`USBPORT_OpenPipe`, `USBH_PdoStartDevice`, `hidusb Status 0`), но
   `newdev/umpnpmgr` сообщает `DevInstallW failed with error 31` и
   `InstallDevice failed`. Те же ошибки видны и для статических USB-устройств
   при загрузке, при этом HID-стек работает. Кандидат в отдельный issue
   (не блокирует T8/T10: bugcheck нет).
5. **IRQ 1 conflict** (`pnpres.c:648`) — известный, повторяется во всех
   прогонах, к вводу фатально не приводит.

## Что осталось pending 0001 / не запускалось

- T9 (функциональный USB HID ввод на desktop) — нужен инжект именно в
  USB-устройство или ручной ввод; monitor `sendkey`/`mouse_move` идут через
  PS/2-путь. Энумерация/биндинг закрыты.
- T5/T6/T7/T11 для **чистой ветки** без test vehicle: без 0001 Live/SETUP-GUI
  падают в `0xc000021a`, поэтому GUI-сценарии проверены только на ISO B.
- Не запускалось: реальное железо (нет PS/2/USB-контроллера на стенде),
  `rostests` для i8042prt (в дереве нет юнит-цели), выключение/перезагрузка
  через Start (не требуется для фичи).

## Артефакты

| Прогон | Каталог | Что внутри |
|---|---|---|
| build | `docs/test-logs/2026-11-09-0004-build.log` | команда, rc, ISO A sha256 |
| T1 + setup | `docs/test-logs/2026-11-09-setup-ps2/` | serial.log, shot-freeloader-menu/keys-*.png |
| T8 в setup | `docs/test-logs/2026-11-09-setup-usb/` | serial.log, shot-keys-*.png |
| T2/T3 baseline | `docs/test-logs/2026-11-09-baseline-live/` | serial.log, shot-bugcheck.png |
| T5/T6/T7/T11 (ISO B) | `docs/test-logs/2026-11-09-tv0001-desktop8/` | serial.log, shot-desktop(-ready).png, shot-run-dialog.png, shot-notepad-*.png, shot-start-menu.png, shot-notepad-drag*.png, shot-window-drag.png |
| T8/T10 hot-plug (ISO B) | `docs/test-logs/2026-11-09-tv0001-usb-hotplug/` | serial.log, serial-hotplug-window.log, shot-045/075/hotplug-final.png |
| test vehicle | `docs/test-logs/2026-11-09-0001-testvehicle-build.log` | патч, команды, ISO B sha256 |

Все `shot-*.ppm` сконвертированы в PNG локально (PIL) и удалены; ISO/PPM в
коммит не попадают.
