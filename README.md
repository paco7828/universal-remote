# Universal Remote

ESP32-based universal infrared remote control with touch screen.
It can **learn** IR signals, **save them to an SD card**, **resend** them,
and transmit **built-in brand codes**.
The entire UI is touch-controlled, featuring scrollable lists and a virtual
keyboard.

---

## Hardware

| Component | Description |
|-----------|-------------|
| MCU | ESP32 (Using FSPI / SPI2 bus) |
| Display | ILI9341 2.4" TFT, 240x320, portrait (rotation 0) |
| Touch | XPT2046 resistive touch, via SPI |
| Storage | MicroSD card |
| IR Transmitter| TSAL6200 |
| IR Receiver | IRM-3638T |

The display and the SD card use **the same SPI bus** (shared MOSI/MISO/CLK),
multiplexed with CS lines. Before sending an IR signal, the code sets `SD_CS` and `TOUCH_CS`
HIGH to avoid bus conflicts.

### Pinout

| Signal | GPIO |
|--------|------|
| TFT CS | 7 |
| TFT RST | 10 |
| TFT DC | 2 |
| TFT MOSI | 6 |
| TFT MISO | 5 |
| TFT CLK | 4 |
| Touch CS | 8 |
| Touch IRQ | 9 |
| SD CS | 3 |
| IR TX | 0 |
| IR RX | 1 |

---

## Dependencies

- **LovyanGFX** - display + touch driver (ILI9341 + XPT2046)
- **IRremoteESP8266** - IR receive / send / decode
- **Preferences** - ESP32 NVS (for saving themes)
- **SD**, **SPI** - Arduino ESP32 core built-in
- **`./IR-codes.h`** - custom header with built-in Pronto Hex codes
  (EPSON and NEC arrays). **Without this, the project will not compile.**

---

## Features

### Signal learning (Receive)

1. `Signal options -> Receive` starts listening mode.
2. The receiver decodes the signal into `decode_results`.
3. The code accepts a **maximum of 3 frames** at once within a signal
   (`MAX_FRAMES = 3`), measuring the pause between them as `gap_ms`.
4. Repeat frames and identical frames are discarded.
5. An `UNKNOWN` (raw) signal is only accepted if it is the **first** frame,
   and contains at least 10 pulses, filtering out noise.
6. The captured signal type is determined automatically:
   - `SIG_VALUE` - known protocol, fixed-length value (e.g., NEC)
   - `SIG_STATE` - AC-like protocols with byte-array states
   - `SIG_RAW` - unknown, raw microsecond array
7. After capture, a **QWERTY keyboard** appears to name the signal
   (max 25 characters, letters + `-`).
8. Saved to the SD card: `/saved-signals/<group>/<name>.txt`.

### Signal sending (Transmit)

- **Saved signals**: `Signal options -> Transmit` -> group list -> signal list -> Send.
  The group is the part of the signal's name before the `-` (e.g., `TV-power` -> `TV` group).
- **Built-in signals**: `Built-in signals` -> brand -> signal -> Send.
- **Long pressing** the `Send` button repeats the signal (except for `SIG_STATE` signals).
- Transmission strategy:
  - `SIG_VALUE`: `irsend.send(protocol, value, bits)` - for RC5/RC6,
    the toggle bit switches automatically on each new press.
  - `SIG_STATE`: `irsend.send(protocol, state, bytes)`.
  - `SIG_RAW`: `irsend.sendRaw(rawData, len, khz)`.
  - If protocol sending returns `false`, but raw data is available, an
    automatic **raw fallback** occurs.
- For multi-frame signals, there is a `gap_ms` pause between frames.

### Built-in signals

The `IRCode` structures declared in `IR-codes.h` store the codes in Pronto Hex format.
The firmware converts them to raw microsecond arrays using the `prontoToRawSignal()`
function and calculates the carrier frequency with `prontoCarrierKHz()`.

Currently, **only these** are available from the UI in the `hardcodedBrands[]` array:

- EPSON
- NEC

(Other brands will only appear if you add them to the array.)

### SD Card Management

- **Info** - total / free / used space + number of saved signal files.
- **Files** - file browser with directory navigation, size display,
  and individual file deletion (`Del` button, **!NO CONFIRMATION MESSAGE!**).
- **Format** - keeps the `/System Volume Information` and `/built-in-signals`
  directories, deletes everything else.
- File list limit: 500 items.

### UI / Themes

Three selectable color themes (persisted in NVS):

- Futuristic Red (default)
- Futuristic Green
- Futuristic Purple

Theme switching is displayed with an animated spreading circle effect.

Other UI features:

- Boot splash: `UNIVERSAL / REMOTE` text for ~2 seconds.
- **Scrollable lists** with a sprite buffer (`LGFX_Sprite`) to prevent flickering.
- **Double tap** on a list item = open.
- **Long hold** on a repeatable button = rapid fire (`REPEAT_INTERVAL = 200 ms`).
- Maximum number of buttons per screen: **30**.
- Button style: corner accents + scanline pattern.

---

## Navigation Structure

```
Main Menu
├── Signal options
│   ├── Transmit  ->  [Group]  ->  [Signal list]  ->  Send
│   └── Receive   ->  Listening... ->  Captured!   ->  Keyboard -> Save
├── Built-in signals
│   └── [Brand]   ->  [Signal list] ->  Send
├── SD Card options
│   ├── Info
│   ├── Files  ->  browser (Up / Back / Del)
│   └── Format ->  Yes, format / Cancel formatting
└── Change theme
    ├── Futuristic Red
    ├── Futuristic Green
    └── Futuristic Purple
```

If SD initialization fails, "SD Card options" disappears from the main menu.

---

## Saved signal file format

**Not binary** - each signal is a simple, line-separated `.txt` file:

```
# UniRemote IR signal
name: TV-power
carrier_khz: 38
frame: 1
gap_ms: 40
protocol: NEC
bits: 32
value: 0x20DF10EF
length: 68
raw: 9000,4500,560,560,...
frame: 2
...
```

Fields per frame:

- `gap_ms` - pause before the next frame (only if there is a next frame)
- `protocol` - name returned by `typeToString()`
- `bits` - number of bits for `SIG_VALUE`, bytes×8 for `SIG_STATE`
- `value` - hex value (`SIG_VALUE`)
- `bytes` - space-separated hex bytes (`SIG_STATE`)
- `length` + `raw` - raw microsecond array (always saved if available)

When loading the file, the code automatically determines the `kind` based
on the existing fields: value -> state -> raw priority.

Folder structure:

```
/saved-signals/
├── TV/
│   ├── power.txt
│   └── mute.txt
├── Misc/
│   └── random-signal.txt
```

Names without `-` are automatically placed in the `Misc` group.

---

## File structure

| File | Role |
|------|------|
| `UniversalRemote.ino` | Full firmware |
| `IR-codes.h` | Built-in Pronto Hex codes (EPSON, NEC arrays, `IRCode` struct) |