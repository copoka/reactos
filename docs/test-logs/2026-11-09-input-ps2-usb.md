# 0004-input-ps2-usb-hid — QEMU input evidence (2026-11-09)

итог: 0 errors в сборке (ISO A rc=0), T1/T3/T4/T8/T10 — PASS; T2 — ожидаемый
baseline 0xC000021A без 0001; T5/T6/T7/T11 — PASS на test vehicle 0001 (в ветку
не входит), для чистой ветки остаются `pending 0001`; T9 не закрыт (USB HID ввод
функционально не инжектился, только энумерация/биндинг).

Повторный прогон **r2** сделан после коммита `156d0ddc7cc` `[KBDHID] Pass the
actual usage count to the scan code translator`: сборка A и B пересобраны
заново (dev сделал `sync`, test vehicle на `ai1` был сброшен), все USB-сценарии
прогнаны заново. `[KBDHID] Failed to translate ... with c0110009` — **0
вхождений** (в прогоне `2026-11-09-setup-usb` их было 20, см. находку 6).

## Стенд

| | |
|---|---|
| хост сборки/QEMU | `ai1` (12 cores, KVM), дерево `/home/l/build/reactos-lab/0004-input-ps2-usb-hid` |
| локальный хост | рабочая станция лаборатории, только подготовка/конвертация PNG |
| QEMU | `qemu-system-i386 -enable-kvm -cpu qemu32 -m 1024 -smp 1 -cdrom ISO -boot d -display none -vga std -monitor unix:...` |
| харнесс | `fork/docs/test-tools/qemu-input-host.sh` (host-side), `fork/docs/test-tools/input-smoke.sh` (localscp-обёртка) |
| режимы | `setup` (FreeLoader → Setup (Debug) → sendkey down/up/ret), `desktop` (Live → wizard → Run→notepad→abc, mouse click/drag), `hotplug` (`--usb`, `device_add usb-mouse` на 60-й секунде) |
| HEAD ветки | `156d0ddc7cc` (3 коммита фичи: i8042prt, kbdhid/mouhid, kbdhid usage count) |

## Команды и rc

```text
# T1 сборка ISO A (локально) — прогон r2, HEAD 156d0ddc7cc
cd /home/l/projects/reactos-lab && scripts/remote-build.sh build 0004-input-ps2-usb-hid bootcd
rc=0   лог: docs/test-logs/2026-11-09-0004-build.log (секция r2)

# T4/T3 + USB HID в текстовом Setup (ISO A) — прогон r2
fork/docs/test-tools/input-smoke.sh 0004-input-ps2-usb-hid setup-usb-r2 --mode setup --usb --timeout 150
rc=0   VERDICT: setup-text-screen,key-down-changed,key-up-changed,key-ret-changed / no-fatal-error

# T5/T6/T7/T11 GUI (ISO B = HEAD 156d0ddc7cc + test vehicle 0001) — прогон r2
fork/docs/test-tools/input-smoke.sh 0004-input-ps2-usb-hid tv0001-desktop-r2 --mode desktop \
  --timeout 180 --iso bootcd-0001-testvehicle.iso
rc=0   VERDICT: drag-moved-window,typing-changed-screen,mouse-click-opened-menu,mouse-drag-moved-notepad / no-fatal-error

# T8/T10 USB HID + hot-plug (ISO B) — прогон r2
fork/docs/test-tools/input-smoke.sh 0004-input-ps2-usb-hid tv0001-usb-hotplug-r2 --mode hotplug --usb \
  --timeout 150 --iso bootcd-0001-testvehicle.iso
rc=0   VERDICT: hotplug-serial-activity / no-fatal-error
```

Прогон r2 использует `--iso bootcd-0001-testvehicle.iso` для ISO B; `input-smoke.sh`
`sync` не делает, поэтому патченое дерево на хосте не сбрасывалось.

Прогоны первого круга (до фикса `156d0ddc7cc`, ISO A/B предыдущей сборки):

```text
fork/docs/test-tools/input-smoke.sh 0004-input-ps2-usb-hid setup-ps2  --mode setup   --timeout 150   rc=0
fork/docs/test-tools/input-smoke.sh 0004-input-ps2-usb-hid setup-usb  --mode setup --usb --timeout 150   rc=0
fork/docs/test-tools/input-smoke.sh 0004-input-ps2-usb-hid baseline-live --mode desktop --timeout 180  rc=1  (bugcheck 0xc000021a — ожидаемо)
fork/docs/test-tools/input-smoke.sh 0004-input-ps2-usb-hid tv0001-desktop8 --mode desktop --timeout 180 --iso bootcd-0001-testvehicle.iso  rc=0
fork/docs/test-tools/input-smoke.sh 0004-input-ps2-usb-hid tv0001-usb-hotplug --mode hotplug --usb --timeout 150 --iso bootcd-0001-testvehicle.iso  rc=0
```

## Test vehicle 0001

Применён только в дереве на `ai1` (`scp` патча `0001-lz32-test-vehicle.patch` →
`patch -p1` → `touch sdk/cmake/CMakeMacros.cmake` → `ninja -j 8 bootcd`),
после патча `sync` не делался. Прогон r2: повторно пересобран на чистом дереве
ветки (ISO A), затем патч, `[0/1] Re-running CMake`, `[6/44] Linking C shared
module dll/win32/lz32/lz32.dll`, `rc=0`, 21.5 c. Подробности:
`docs/test-logs/2026-11-09-0001-testvehicle-build.log` (секция r2).

## ISO

| | ISO A (ветка) | ISO B (HEAD + test vehicle 0001) |
|---|---|---|
| HEAD | `156d0ddc7cc` | `156d0ddc7cc` + patch 0001 |
| path | `output-MinGW-i386/bootcd.iso` | `output-MinGW-i386/bootcd-0001-testvehicle.iso` |
| size | 373293056 (356 MiB) | 373293056 (356 MiB) |
| mtime (UTC) | 2026-11-09 18:14:19 | 2026-11-09 18:15:55 |
| sha256 | `22457bf612e1c62bbecaf15a6eafd0b109fb2c36d3722748261e4ca708ff6dc6` | `5d7da4d811cbf22f99499510674c87b7cdd778c3bc592260a6039d4306ba475e` |

Прежние ISO первого круга (для сравнения): A `10e9492793c4b44dd4b0daf423b01efe99d15539676264401628907ffbf86061`,
B `83bd47a744c04cd55d8172b9f2f082060f793fbcef1ae8c080c60e4ed75dd306`.
ISO B в ветку не входит и в git не попадает; после применения патча `bootcd.iso`
на `ai1` тоже стал B.

## Матрица

| # | Сценарий | Итог | Доказательство |
|---|---|---|---|
| T1 | Сборка `bootcd` | **PASS** rc=0 (r2: ninja 7 шагов, ISO пересобран) | `2026-11-09-0004-build.log`, sha256 выше |
| T2 | Live без bugcheck | **baseline (ожидаемо FAIL без 0001)** `Fatal System Error: 0xc000021a` в smss (0xC000036F) | `2026-11-09-baseline-live/serial.log:140` |
| T3 | PS/2 драйверы в serial | **PASS**: `i8042prt.sys`, `kbdclass.sys`, `mouclass.sys` (r2 desktop: `i8042prt` 2, `kbdclass` 1, `mouclass` 1) | `2026-11-09-tv0001-desktop-r2/serial.log:110,112,114`; `2026-11-09-baseline-live/serial.log:110,112,114` |
| T4 | PS/2 клавиатура в Setup Text Mode | **PASS** (r2 подтверждён): Down/Up/Enter меняют экран, 6.744% / 3.043% / 8.614% пикселей | `2026-11-09-setup-usb-r2/shot-keys-*.png`, `harness.log` |
| T5 | Desktop: Win+R → notepad → `abc` | **PASS на ISO B**, `pending 0001` для чистой ветки: в Notepad `abc`, Line 1 col 4 (r2) | `2026-11-09-tv0001-desktop-r2/shot-notepad-abc.png` |
| T6 | Мышь: move/click/drag | **PASS на ISO B**, `pending 0001`: клик открыл Start (56037 px / 11.674%), перетащено окно Notepad (164080 px / 34.183%), перетащено окно LiveCD (142297 px / 29.645%) — r2 совпал с первым кругом | `2026-11-09-tv0001-desktop-r2/shot-start-menu.png`, `shot-notepad-dragged.png`, `shot-window-drag.png` |
| T7 | Нет залипших клавиш/дребезга | **PASS на ISO B**, `pending 0001`: `sendkey a,b,c` → ровно 3 символа (колонка 4); после клика `d` — колонка 5 | `shot-notepad-abc.png`, `shot-notepad-after-click.png` |
| T8 | USB HID энумерация/биндинг | **PASS** (r2, уточнено): `hidusb.sys` + `hidclass.sys` + `kbdhid.sys` (+`mouhid.sys` в Live), `hidusb.c:1418 Status 0` ×2, usbhub PnP `Installing: HID\Vid_0627&Pid_0001`, нет bugcheck, **0** `[KBDHID] Failed to translate`/`c0110009` (было 20). `usbuhci.sys` в serial не логируется (находка 3). В desktop/hotplug-r2 остаются 2 строки `Failed to translate` от `pnpres.c:1057/1189` для `Root\*PNP0501` (COM1) — к HID не относятся | `2026-11-09-setup-usb-r2/serial.log:151,152,158,159,164`; `2026-11-09-tv0001-usb-hotplug-r2/serial.log:505` |
| T9 | USB HID ввод на desktop | **pending 0001 / не закрыт**: ввод через monitor идёт в PS/2-путь; отдельного USB-инжекта нет. Энумерация и биндинг подтверждены (T8) | — |
| T10 | Hot-plug USB-мыши | **PASS (с находкой 4)** (r2): на 60-й секунде `device_add usb-mouse` → `USB\Vid_0627&Pid_0001\5&c3e83e4b&2`, `USBPORT_OpenPipe`, `USBH_PdoStartDevice`, `hidusb Status 0`, bugcheck нет, **0** `Failed to translate`/`c0110009`; далее `Installing: HID\…\6&dc3a5f00&0001`; динамическая установка `DevInstallW failed with error 31` (как и прежде) | `2026-11-09-tv0001-usb-hotplug-r2/serial.log:493-509`, `serial-hotplug-window.log` |
| T11 | US-раскладка по умолчанию | **PASS на ISO B**, `pending 0001`: `abc` → `abc` (US) | `shot-notepad-abc.png` |
| R1 | Setup доходит до выбора языка | **PASS**: "Language Selection", ENTER = Continue | `2026-11-09-setup-ps2/shot-keys-before.png`, `2026-11-09-setup-usb-r2/shot-keys-before.png` |
| R2 | Регрессии ввода/USB/загрузки | r2: новых bugcheck нет, PS/2-ввод на ISO B не сломан правкой kbdhid (abc/Start/drag те же, что до фикса); IRQ conflict — известный; новые ERR — те же, что в первом круге | serial-логи прогонов |

## Ключевые строки serial

```text
# Setup (Debug), ISO A r2 (sha256 2245…6dc6) — PS/2 и USB HID
Loading: \SystemRoot\system32\drivers\i8042prt.sys at AF0FE000 with 1d pages       # :146
Loading: \SystemRoot\system32\drivers\hidusb.sys at AFFA9000 with d pages          # :151
Loading: \SystemRoot\system32\drivers\hidclass.sys at AF0EA000 with 14 pages      # :152
(/drivers/hid/hidusb/hidusb.c:1418) Status 0                                      # :158
Loading: \SystemRoot\system32\drivers\kbdhid.sys at AFF9D000 with c pages         # :159
(/drivers/hid/hidusb/hidusb.c:1418) Status 0                                      # :164
# [KBDHID] Failed to translate / c0110009 — 0 вхождений
# mouclass.sys/mouhid.sys в MININT отсутствуют (0xc0000034), kbdclass не грузится — usetup читает i8042prt напрямую

# Live (Debug) r2 на ISO B — desktop (T5/T6/T7/T11)
Loading: ... i8042prt.sys at AFF9C000 / kbdclass.sys at AF3B2000                  # :110,112
Failed to translate interrupt resource (Vector: 0x4 | Level: 0x4)                 # :352  pnpres, COM1 — не HID
Failed to translate resources for Root\*PNP0501\0000                              # :353  pnpres, COM1 — не HID

# Live (Debug) + --usb r2 на ISO B — T8/T10
Installing: USB\Vid_0627&Pid_0001\5&c3e83e4b&2        # hot-plug, :503
USBPORT_OpenPipe: DeviceHandle - B28592A0, ...        # :504
(/drivers/hid/hidusb/hidusb.c:1418) Status 0          # :505
err:(/dll/win32/newdev/newdev.c:1039) DevInstallW failed with error 31   # :507
Installing: HID\Vid_0627&Pid_0001\6&dc3a5f00&0001     # :509

# Для сравнения — первый круг, ISO A до фикса (2026-11-09-setup-usb/serial.log)
(/drivers/hid/kbdhid/kbdhid.c:186) [KBDHID] Failed to translate break usage list with c0110009   # ×20 всего
(/drivers/hid/kbdhid/kbdhid.c:198) [KBDHID] Failed to translate make usage list with c0110009
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
   (baseline-прогон и r2 desktop).
3. **`usbuhci.sys` не появляется в serial** ни в одном прогоне (0 вхождений),
   хотя UHCI-контроллер работает: usbhub перечисляет устройства, HID
   устанавливается. Вероятно, загрузка минипорта не логируется — это не
   дефект фичи, но критерий T8 «usbuhci в serial» формально не выполним в этом
   build; энумерация доказана строками usbhub/hidusb.
4. **Hot-plug: ошибка установки 31.** Новое устройство энумерируется и
   стартует (`USBPORT_OpenPipe`, `USBH_PdoStartDevice`, `hidusb Status 0`), но
   `newdev/umpnpmgr` сообщает `DevInstallW failed with error 31` и
   `InstallDevice failed`. Те же ошибки видны и для статических USB-устройств
   при загрузке, при этом HID-стек работает. В r2 поведение не изменилось.
   Кандидат в отдельный issue (не блокирует T8/T10: bugcheck нет).
5. **IRQ conflict** (`pnpres.c:648`, IRQ 0x1/0xe/0xf) — известный, повторяется
   во всех прогонах, к вводу фатально не приводит.
6. **[KBDHID] ложный `HIDP_STATUS_I8042_TRANS_UNKNOWN` (c0110009) — исправлено
   в `156d0ddc7cc`, проверено в r2.**
   - *Было* (ISO A `10e9492…`, прогон `2026-11-09-setup-usb`): на каждом
     HID-отчёте USB-клавиатуры `kbdhid.c:186/198 [KBDHID] Failed to translate
     break/make usage list with c0110009` — 20 вхождений. Причина:
     `KbdHid_ReadCompletion` передавал в
     `HidP_TranslateUsageAndPagesToI8042ScanCodes` ёмкость списков
     (`DeviceExtension->UsageListLength`), а API ждёт **фактическое** число
     заполненных элементов. `HidP_UsageAndPageListDifference` обнуляет хвост
     списков, транслятор доходил до пустого элемента (UsagePage 0) и возвращал
     `HIDP_STATUS_I8042_TRANS_UNKNOWN`, хотя все реальные usage были
     транслированы. Статус был ложным: ввод при этом работал.
   - *Стало* (ISO A `22457bf…`, прогон `2026-11-09-setup-usb-r2`): хелпер
     `KbdHid_GetUsageCount` считает элементы до первого пустого слота, в
     транслятор идут `BreakCount`/`MakeCount`. `[KBDHID] Failed to translate` —
     **0**, `c0110009` — **0**, при этом `hidusb Status 0` и привязка
     `kbdhid` на месте. Проверка статуса и её отладочный вывод сохранены, так
     что настоящий сбой трансляции по-прежнему будет виден в логе.
   - Не путать с остающимися 2 строками `Failed to translate interrupt
     resource` / `Failed to translate resources for Root\*PNP0501` — это
     `pnpres.c` и COM1, к HID-стеку отношения не имеют (есть и в первом круге).

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
| build A (r2, `2245…6dc6`) | `docs/test-logs/2026-11-09-0004-build.log` | секция r2: команда, rc, ISO A sha256 |
| build B (r2, `5d7d…475e`) | `docs/test-logs/2026-11-09-0001-testvehicle-build.log` | секция r2: патч, команды, ISO B sha256 |
| T4/T3 + USB HID setup (r2) | `docs/test-logs/2026-11-09-setup-usb-r2/` | serial.log, harness.log, shot-freeloader-menu/setup-selected/keys-*.png |
| T5/T6/T7/T11 desktop (ISO B, r2) | `docs/test-logs/2026-11-09-tv0001-desktop-r2/` | serial.log, shot-desktop(-ready)(-final).png, shot-run-dialog.png, shot-notepad-*.png, shot-start-menu.png, shot-window-drag.png |
| T8/T10 hot-plug (ISO B, r2) | `docs/test-logs/2026-11-09-tv0001-usb-hotplug-r2/` | serial.log, serial-hotplug-window.log, shot-045/075/hotplug-final.png |
| build (круг 1) | `docs/test-logs/2026-11-09-0004-build.log` | ISO A `10e9…6061` |
| T1 + setup (круг 1) | `docs/test-logs/2026-11-09-setup-ps2/` | serial.log, shot-freeloader-menu/keys-*.png |
| T8 в setup (круг 1, с c0110009) | `docs/test-logs/2026-11-09-setup-usb/` | serial.log, shot-keys-*.png |
| T2/T3 baseline | `docs/test-logs/2026-11-09-baseline-live/` | serial.log, shot-bugcheck.png |
| T5/T6/T7/T11 (круг 1) | `docs/test-logs/2026-11-09-tv0001-desktop8/` | serial.log, shot-desktop(-ready).png, shot-run-dialog.png, shot-notepad-*.png, shot-start-menu.png, shot-notepad-drag*.png, shot-window-drag.png |
| T8/T10 hot-plug (круг 1) | `docs/test-logs/2026-11-09-tv0001-usb-hotplug/` | serial.log, serial-hotplug-window.log, shot-045/075/hotplug-final.png |
| test vehicle | `docs/test-logs/2026-11-09-0001-testvehicle-build.log` | патч, команды, ISO B sha256 (круг 1 и r2) |

Все `shot-*.ppm` сконвертированы в PNG локально (PIL) и удалены; ISO/PPM в
коммит не попадают.
