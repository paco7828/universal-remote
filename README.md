# Universal Remote

An ESP32-based touchscreen universal IR remote controller with SD card storage, built-in hardcoded brand signals, and a futuristic themed UI.

---

## Hardware

| Component | Description |
|-----------|-------------|
| MCU | ESP32 (FSPI bus) |
| Display | ILI9341 2.4" TFT (240x320, portrait) |
| Touch | XPT2046 resistive touchscreen |
| Storage | MicroSD card (FAT32) |
| IR Transmitter | Connected to GPIO 0 |
| IR Receiver | Connected to GPIO 1 |

### Pin Assignment

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

## Features

### Signal Transmission

- **Built-in brand signals** - Hardcoded IR codes for common projector and LED strip brands:
  - Epson (Freeze, Power On, Power Off, Power Toggle)
  - Acer (Freeze, Power Toggle)
  - BenQ (Freeze, Power On, Power Off, Power Toggle)
  - NEC (Freeze, Power On, Power Off, Power Toggle)
  - Panasonic (Freeze, Power On, Power Off, Power Toggle)
  - LED Strip (On, Off)
- **Saved signals** - Custom captured signals stored on the SD card as `.bin` files
- Signal files are grouped by name prefix for easier navigation (e.g. `TV-power.bin`, `TV-mute.bin` appear under the `TV` group)

### Signal Capture

- Captures raw IR signals via the receiver
- Minimum signal length validation (rejects noise/invalid captures)
- Keyboard UI for naming captured signals (up to 25 characters)
- Signals saved to `/saved-signals/` on the SD card

### SD Card Management

- Storage info screen (total, used, free space + file counts per directory)
- Full file browser with directory navigation
- Individual file deletion
- SD format function (preserves `/built-in-signals/` directory)

### UI / Themes

- Three switchable color themes:
  - Futuristic Red (default)
  - Futuristic Green
  - Futuristic Purple
- Futuristic button style with corner accents and scan-line fill effect
- All navigation is touch-based

---

## Navigation Structure

```
Main Menu
├── Signal options
│   ├── Transmit
│   │   └── Saved signals
│   │       └── [Group] -> [Signal list] -> Send
│   └── Receive
│       └── Listening... -> Capture -> Name (keyboard) -> Save
├── Built-in signals
│   └── [Brand] -> [Signal list] -> Send
├── SD Card options
│   ├── Info
│   ├── Files (browser + delete)
│   └── Format
└── Change theme
    ├── Futuristic Red
    ├── Futuristic Green
    └── Futuristic Purple
```

---

## IR Signal Format

Signals are stored and transmitted as raw microsecond-duration arrays using the IRremote library's `sendRaw()` method at 38 kHz carrier frequency.

Built-in hardcoded signals use Pronto Hex format internally (`IR-codes.h`) and are converted to raw microsecond arrays at runtime via `prontoToRaw()`.

Saved signals on SD are binary-serialized `IRSignal` structs:

```cpp
struct IRSignal {
    char name[26];        // Signal name (null-terminated)
    uint16_t rawData[200]; // Raw durations in microseconds
    uint8_t rawDataLen;   // Number of valid entries
} __attribute__((packed));
```

---

## Dependencies

- `Adafruit_ILI9341`
- `Adafruit_GFX`
- `XPT2046_Touchscreen`
- `IRremote`
- `SD` (built-in ESP32 Arduino)
- `SPI` (built-in)

---

## Known Limitations

- SD card and display share the same FSPI bus; SD CS is deselected before IR transmission to avoid bus conflicts
- Touch calibration constants (`TS_MINX`, `TS_MINY`, `TS_MAXX`, `TS_MAXY`) may need adjustment per panel
- Maximum 30 touch buttons rendered per screen
- Signal name maximum length: 25 characters (alphanumeric + `-`)
