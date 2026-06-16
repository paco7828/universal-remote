#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <SPI.h>
#include <SD.h>
#include <IRremote.hpp>
#include "./IR-codes.h"

// Pin definíciók – IDE kerülnek, az LGFX class előtt kell legyenek
constexpr uint8_t TFT_CS = 7;
constexpr uint8_t TFT_RST = 10;
constexpr uint8_t TFT_DC = 2;
constexpr uint8_t TFT_MOSI = 6;
constexpr uint8_t TFT_MISO = 5;
constexpr uint8_t TFT_CLK = 4;
constexpr uint8_t TOUCH_CS = 8;
constexpr uint8_t TOUCH_IRQ = 9;
constexpr uint8_t SD_CS = 3;
constexpr uint8_t IR_TX = 0;
constexpr uint8_t IR_RX = 1;

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Touch_XPT2046 _touch_instance;
public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.pin_sclk = TFT_CLK;
      cfg.pin_mosi = TFT_MOSI;
      cfg.pin_miso = -1;  // display SDO nem kell
      cfg.pin_dc = TFT_DC;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = TFT_CS;
      cfg.pin_rst = TFT_RST;
      cfg.panel_width = 240;
      cfg.panel_height = 320;
      _panel_instance.config(cfg);
    }
    {
      auto cfg = _touch_instance.config();
      cfg.x_min = 3850;
      cfg.x_max = 240;
      cfg.y_min = 250;
      cfg.y_max = 3750;
      cfg.pin_sclk = TFT_CLK;
      cfg.pin_mosi = TFT_MOSI;
      cfg.pin_miso = TFT_MISO;
      cfg.pin_cs = TOUCH_CS;
      cfg.pin_int = TOUCH_IRQ;
      cfg.bus_shared = true;
      cfg.spi_host = SPI2_HOST;
      cfg.freq = 1000000;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }
    setPanel(&_panel_instance);
  }
};

constexpr int MAX_SAVED_SIGNAL_CHARS = 25;

// ===================================================================================
// ================================= STRUCTS =========================================
// ===================================================================================

// IR Signal structure for SD card storage
struct IRSignal {
  char name[MAX_SAVED_SIGNAL_CHARS + 1];
  uint16_t rawData[200];
  uint8_t rawDataLen;
} __attribute__((packed));

// Touchable button structure
struct TouchButton {
  int x, y, w, h;
  uint16_t color;
  uint16_t textColor;
  const char *label;
  void (*callback)();
  bool pressed;
  bool isBackButton;
  bool repeatable;
};

// Option structure with name and callback
struct Option {
  const char *name;
  void (*callback)();
};

// Theme color sets
struct ThemeColors {
  uint16_t primary;
  uint16_t secondary;
  uint16_t accent;
  uint16_t dark;
  uint16_t darkest;
};

struct HardcodedBrand {
  const char *brandName;
  const IRCode *codes;
  uint8_t codesLength;
};

uint8_t activeBtnIndex = 0;
const IRCode *currentBrandCodes = nullptr;
uint8_t currentBrandCodesLength = 0;
int currentCodeIndex = 0;

// File browser variables
String sdFiles[500];
int sdFileCount = 0;
int sdHighlightedIndex = 0;
int sdScrollOffset = 0;
bool onSDFiles = false;
String currentPath = "/";
int sdMaxVisible = 6;

String builtInBrands[50];
int builtInBrandCount = 0;
int builtInBrandHighlightedIndex = 0;
int builtInBrandScrollOffset = 0;
bool onBuiltInBrands = false;

String builtInSignals[50];
int builtInSignalCount = 0;
int builtInSignalHighlightedIndex = 0;
int builtInSignalScrollOffset = 0;
bool onBuiltInSignals = false;
String currentBrandPath = "";

// Signal grouping
String savedSignalGroups[50];
int savedSignalGroupCount = 0;
int savedSignalGroupHighlightedIndex = 0;
int savedSignalGroupScrollOffset = 0;
bool onSavedSignalGroups = false;
String currentSavedGroup = "";

String groupedSignalFiles[50];
int groupedSignalCount = 0;
int groupedSignalHighlightedIndex = 0;
int groupedSignalScrollOffset = 0;

// ===================================================================================
// ================================== THEMES =========================================
// ===================================================================================

// Define theme presets
const ThemeColors THEME_FUTURISTIC_RED = {
  0xF800,  // Red
  0xD000,  // Crimson
  0x8800,  // Dark Red
  0x4208,  // Grey
  0x2104   // Dark Grey
};

const ThemeColors THEME_FUTURISTIC_GREEN = {
  0x07E0,  // Green
  0x05C0,  // Dark Green
  0x0400,  // Darker Green
  0x4208,  // Grey
  0x2104   // Dark Grey
};

const ThemeColors THEME_FUTURISTIC_PURPLE = {
  0xF81F,  // Magenta/Purple
  0xC81F,  // Dark Purple
  0x8010,  // Darker Purple
  0x4208,  // Grey
  0x2104   // Dark Grey
};

// ===================================================================================
// ========================= FUNCTION PROROTYPES =====================================
// ===================================================================================

// Display helper
void initDisplay();
void drawMenuUI();
void drawBackBtn(uint8_t x, uint8_t y, uint8_t width, uint8_t height, void (*callback)());
void drawTitle(const char *titleName, uint16_t x = 90, uint16_t y = 305);
int processTouchButtons(int tx, int ty);
void drawButton(TouchButton *btn, bool active);
void drawHeaderFooter();
void createTouchBox(int x, int y, int width, int height, uint16_t color, uint16_t textColor, const char *label, void (*callback)(), bool isBackButton = false, bool repeatable = false);
bool isTouchInButton(TouchButton *btn, int tx, int ty);
void createOptions(const Option options[], int count, int x = 10, int y = 50, int btnWidth = 220, int btnHeight = 45);
void createGridOptions(const Option options[], int count, int rows, int cols, int startX = 10, int startY = 70, int spacing = 5, int btnSize = 50);

// Signal options
void signalOptions();
void transmitOptions();
void listSavedSignals();
void listFavoriteSignals();
void startSignalListen();
void drawKeyboard();
void processKeyboardInput();

// Projector brands
void builtInSignalsBrowser();

// IR Signal functions
void captureSignal();
void saveSignalToSD(const IRSignal &signal);
void loadSignalsFromSD();
void transmitSignal(const IRSignal &signal);
uint16_t prontoToRawSignal(const uint16_t *pronto, uint16_t *outRaw, uint8_t maxOut);
void deleteSignalFromSD(const char *filename);

// SD Card
void sdData();
void listSDInfo();
String formatBytes(uint64_t bytes);
int countFilesInDirectory(const char *directoryPath);
void listSDFiles();
void sdFormatOptions();
bool deleteDirectory(const char *path);
void formatSD();
String extractPrefix(String filename);
void listGroupedSignals();
void deleteSelectedFile();

// Theme
void themeOptions();
void setTheme(uint8_t themeIndex);
void setThemeFuturisticRed();
void setThemeFuturisticGreen();
void setThemeFuturisticPurple();

// ===================================================================================
// ================================= OPTIONS =========================================
// ===================================================================================

// Menu options (first glance)
const Option MENU_OPTIONS[] = {
  { "Signal options", signalOptions },
  { "Built-in signals", builtInSignalsBrowser },
  { "SD Card options", sdData },
  { "Change theme", themeOptions }
};

const Option MENU_OPTIONS_NO_SD[] = {
  { "Signal options", signalOptions },
  { "Built-in signals", builtInSignalsBrowser },
  { "Change theme", themeOptions }
};

// Signal options
const Option SIGNAL_OPTIONS[] = {
  { "Transmit", transmitOptions },
  { "Receive", startSignalListen },
  { "Back", signalOptions }
};

const Option TRANSMIT_OPTIONS[] = {
  { "Saved signals", listSavedSignals },
  { "Favorites", listFavoriteSignals },
  { "Back", signalOptions }
};

// SD Card
const Option SD_CARD_OPTIONS[] = {
  { "Info", listSDInfo },
  { "Files", listSDFiles },
  { "Format", sdFormatOptions },
  { "Back", drawMenuUI }
};

const Option SD_FORMAT_OPTIONS[] = {
  { "Yes, format", formatSD },
  { "Cancel formatting", sdData }
};

// Theme
const Option THEME_OPTIONS[] = {
  { "Futuristic Red", setThemeFuturisticRed },
  { "Futuristic Green", setThemeFuturisticGreen },
  { "Futuristic Purple", setThemeFuturisticPurple },
  { "Back", drawMenuUI }
};

// Hardcoded signals
const HardcodedBrand hardcodedBrands[] = {
  { "EPSON", EPSON_CODES, EPSON_CODES_LENGTH },
  { "LED_STRIP", LED_STRIP_CODES, LED_STRIP_CODES_LENGTH },
  { "ACER", ACER_CODES, ACER_CODES_LENGTH },
  { "BENQ", BENQ_CODES, BENQ_CODES_LENGTH },
  { "NEC", NEC_CODES, NEC_CODES_LENGTH },
  { "PANASONIC", PANASONIC_CODES, PANASONIC_CODES_LENGTH }
};

const uint8_t hardcodedBrandsLength = sizeof(hardcodedBrands) / sizeof(hardcodedBrands[0]);

// ===================================================================================
// ============================= INITIALIZATION ======================================
// ===================================================================================

// Initialize display & touch & SD
LGFX tft;
SPIClass spiSD(FSPI);
ThemeColors currentTheme;

// Helper variables
TouchButton buttons[30];  // Max number of buttons as predefined array
uint8_t buttonCount = 0;
bool initializedSD = false;

// Default theme
const ThemeColors DEFAULT_THEME = THEME_FUTURISTIC_RED;

// Debouncing variables
unsigned long lastTouchTime = 0;
constexpr unsigned long DEBOUNCE_DELAY = 300;  // ms

// IR Signal variables
uint16_t currentRawData[200];
uint32_t currentDecodedHex = 0;
uint8_t currentRawDataLen = 0;
bool signalCaptured = false;
bool listeningForSignal = false;
int mark_excess_micros = 20;

// Keyboard variables
const char *const alphabet[26] = {
  "A", "B", "C", "D", "E", "F", "G",
  "H", "I", "J", "K", "L", "M", "N",
  "O", "P", "Q", "R", "S", "T", "U",
  "V", "W", "X", "Y", "Z"
};

int cursorRow = 0;
int cursorCol = 0;
char outputText[MAX_SAVED_SIGNAL_CHARS + 1] = "";
bool onKeyboard = false;

// Signal list variables
String signalFiles[50];
int signalCount = 0;
int highlightedIndex = 0;
int scrollOffset = 0;
bool onSavedSignals = false;

// ===================================================================================
// =================================== SETUP =========================================
// ===================================================================================

void setup() {
  initDisplay();
}

// ===================================================================================
// ==================================== LOOP =========================================
// ===================================================================================

bool touchHeld = false;
int heldButtonIndex = -1;
unsigned long lastRepeatFire = 0;
constexpr unsigned long REPEAT_INTERVAL = 200;  // ms

void loop() {
  if (listeningForSignal && !signalCaptured) {
    if (IrReceiver.decode()) {
      captureSignal();
      if (currentRawDataLen < 10) {
        buttonCount = 0;
        tft.fillRect(0, 60, 240, 258, TFT_BLACK);
        printCentered("Invalid", 120, currentTheme.primary, 2);
        printCentered("signal!", 140, currentTheme.primary, 2);
        delay(2000);
        startSignalListen();
      } else {
        signalCaptured = true;
        listeningForSignal = false;
        tft.fillRect(0, 60, 240, 258, TFT_BLACK);
        tft.setTextSize(2);
        tft.setTextColor(currentTheme.primary);
        tft.setCursor(65, 150);
        tft.println("Captured!");
        delay(1500);
        drawKeyboard();
      }
      IrReceiver.resume();
    }
  }

  int32_t tx, ty;
  bool touching = tft.getTouch(&tx, &ty);

  if (touching) {
    if (!touchHeld) {
      touchHeld = true;
      heldButtonIndex = processTouchButtons((int)tx, (int)ty);
      lastRepeatFire = millis();
    } else if (heldButtonIndex >= 0 && heldButtonIndex < buttonCount) {
      TouchButton *btn = &buttons[heldButtonIndex];
      if (btn->repeatable && isTouchInButton(btn, (int)tx, (int)ty)) {
        unsigned long now = millis();
        if (now - lastRepeatFire > REPEAT_INTERVAL) {
          lastRepeatFire = now;
          if (btn->callback) btn->callback();
          // a callback újrarajzolt -> frissítsük a "lenyomva" vizuált
          if (heldButtonIndex < buttonCount) {
            TouchButton *refreshed = &buttons[heldButtonIndex];
            refreshed->pressed = true;
            drawButton(refreshed, true);
          }
        }
      }
    }
    delay(50);
  } else {
    touchHeld = false;
    heldButtonIndex = -1;
    for (int i = 0; i < buttonCount; i++) {
      if (buttons[i].pressed) {
        buttons[i].pressed = false;
        drawButton(&buttons[i], false);
      }
    }
  }

  delay(10);
}

// ===================================================================================
// ============================= DISPLAY DRAWING =====================================
// ===================================================================================

void initDisplay() {
  Serial.begin(115200);

  currentTheme = DEFAULT_THEME;

  IrSender.begin(IR_TX);
  IrReceiver.begin(IR_RX);

  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  spiSD.begin(TFT_CLK, TFT_MISO, TFT_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spiSD, 20000000)) {
    initializedSD = false;
  } else {
    initializedSD = true;
    if (!SD.exists("/saved-signals")) SD.mkdir("/saved-signals");
  }

  tft.init();
  tft.setRotation(0);

  drawMenuUI();
}

void drawTitle(const char *titleName, uint16_t x, uint16_t y) {
  tft.setTextSize(1);
  tft.setCursor(x, y);
  tft.setTextColor(currentTheme.primary);
  tft.println(titleName);
}

void printCentered(const char *text, int y, uint16_t color, uint8_t size) {
  tft.setTextSize(size);
  tft.setTextColor(color);
  int16_t tw = tft.textWidth(text);
  int16_t x = (240 - tw) / 2;
  tft.setCursor(x, y);
  tft.print(text);
}

// Draw header and footer decorations
void drawHeaderFooter() {
  // Header border
  tft.drawFastHLine(0, 0, 239, currentTheme.primary);

  // Top corner accents
  for (int i = 0; i < 15; i++) {
    tft.drawPixel(i, 2 + i / 3, currentTheme.primary);
    tft.drawPixel(239 - i, 2 + i / 3, currentTheme.primary);
  }

  // Footer border
  tft.drawFastHLine(0, 318, 240, currentTheme.primary);

  // Bottom corner accents
  for (int i = 0; i < 15; i++) {
    tft.drawPixel(i, 317 - i / 3, currentTheme.primary);
    tft.drawPixel(239 - i, 317 - i / 3, currentTheme.primary);
  }
}

// Redraw the entire UI with current theme
void drawMenuUI() {
  // Clear screen
  tft.fillScreen(TFT_BLACK);

  // Draw header and footer
  drawHeaderFooter();

  // Main title
  tft.setTextColor(currentTheme.primary);
  tft.setTextSize(3);
  tft.setCursor(45, 10);
  tft.println("UNIVERSAL");
  tft.setCursor(70, 40);
  tft.println("REMOTE");

  // Draw menu options & title
  if (initializedSD) {
    createOptions(MENU_OPTIONS, 4, 10, 70);
  } else {
    createOptions(MENU_OPTIONS_NO_SD, 3, 10, 80);
  }
  drawTitle("MENU", 110);
}

void drawBackBtn(uint8_t x, uint8_t y, uint8_t width, uint8_t height, void (*callback)()) {
  createTouchBox(x, y, width, height, currentTheme.secondary, currentTheme.secondary, "Back", callback, true);
}

// ===================================================================================
// ============================= DISPLAY OPTIONS =====================================
// ===================================================================================

void signalOptions() {
  buttonCount = 0;
  tft.fillRect(0, 60, 240, 258, TFT_BLACK);

  // Create 1x2 grid - TRANSMIT goes directly to saved signals
  int btnSize = 100;
  int spacing = 10;
  int totalWidth = (btnSize * 2) + spacing;
  int startX = (240 - totalWidth) / 2;

  createTouchBox(startX, 70, btnSize, btnSize, currentTheme.primary, currentTheme.primary, "Transmit", listSavedSignals);
  createTouchBox(startX + btnSize + spacing, 70, btnSize, btnSize, currentTheme.primary, currentTheme.primary, "Receive", startSignalListen);

  // Centered Back button
  createTouchBox(60, 190, 120, 45, currentTheme.secondary, currentTheme.secondary, "Back", drawMenuUI, true);

  drawHeaderFooter();

  drawTitle("Signal options", 80);
}

void transmitOptions() {
  createOptions(TRANSMIT_OPTIONS, 3, 10, 100);
  drawTitle("Signal > Transmit", 70);
}

// FIXED: Removed needsFullRedraw optimization
void listSavedSignals() {
  onSavedSignalGroups = true;
  onSavedSignals = false;
  savedSignalGroupCount = 0;

  File dir = SD.open("/saved-signals");
  if (dir) {
    File entry = dir.openNextFile();
    while (entry && savedSignalGroupCount < 50) {
      if (!entry.isDirectory()) {
        String filename = String(entry.name());
        String prefix = extractPrefix(filename);
        bool exists = false;
        for (int i = 0; i < savedSignalGroupCount; i++) {
          if (savedSignalGroups[i] == prefix) {
            exists = true;
            break;
          }
        }
        if (!exists) savedSignalGroups[savedSignalGroupCount++] = prefix;
      }
      entry.close();
      entry = dir.openNextFile();
    }
    dir.close();
  }

  savedSignalGroupHighlightedIndex = 0;
  savedSignalGroupScrollOffset = 0;
  drawSavedSignalsList();
}

void drawSavedSignalsList() {
  buttonCount = 0;
  tft.fillRect(0, 60, 240, 258, TFT_BLACK);
  drawHeaderFooter();

  if (savedSignalGroupCount == 0) {
    printCentered("No signals", 150, currentTheme.primary, 2);
    printCentered("saved!", 170, currentTheme.primary, 2);
    drawBackBtn(60, 200, 120, 40, signalOptions);
    drawTitle("Transmit > Saved", 70);
    return;
  }

  int maxVisible = 5, yPos = 70, lineHeight = 30;
  for (int i = savedSignalGroupScrollOffset; i < min(savedSignalGroupScrollOffset + maxVisible, savedSignalGroupCount); i++) {
    if (i == savedSignalGroupHighlightedIndex) {
      tft.fillRect(5, yPos, 230, 25, currentTheme.primary);
      tft.setTextColor(TFT_BLACK);
    } else {
      tft.setTextColor(TFT_WHITE);
    }
    tft.setTextSize(2);
    tft.setCursor(10, yPos + 5);
    tft.println(savedSignalGroups[i]);
    yPos += lineHeight;
  }

  if (savedSignalGroupScrollOffset > 0)
    tft.fillTriangle(225, 85, 220, 90, 230, 90, TFT_WHITE);
  if (savedSignalGroupScrollOffset + maxVisible < savedSignalGroupCount)
    tft.fillTriangle(225, 225, 220, 220, 230, 220, TFT_WHITE);

  createTouchBox(
    10, 235, 70, 30, currentTheme.secondary, currentTheme.secondary, "Up", []() {
      if (savedSignalGroupHighlightedIndex > 0) {
        savedSignalGroupHighlightedIndex--;
        if (savedSignalGroupHighlightedIndex < savedSignalGroupScrollOffset)
          savedSignalGroupScrollOffset = savedSignalGroupHighlightedIndex;
        drawSavedSignalsList();
      }
    },
    false, true);

  createTouchBox(
    85, 235, 70, 30, currentTheme.secondary, currentTheme.secondary, "Down", []() {
      if (savedSignalGroupHighlightedIndex < savedSignalGroupCount - 1) {
        savedSignalGroupHighlightedIndex++;
        if (savedSignalGroupHighlightedIndex >= savedSignalGroupScrollOffset + 5)
          savedSignalGroupScrollOffset = savedSignalGroupHighlightedIndex - 5 + 1;
        drawSavedSignalsList();
      }
    },
    false, true);

  createTouchBox(160, 235, 70, 30, currentTheme.primary, currentTheme.primary, "Open", []() {
    currentSavedGroup = savedSignalGroups[savedSignalGroupHighlightedIndex];
    listGroupedSignals();  // új csoport -> friss betöltés indokolt
  });

  tft.fillRect(10, 270, 220, 25, TFT_BLACK);
  createTouchBox(10, 270, 220, 25, currentTheme.secondary, currentTheme.secondary, "Back", signalOptions, true);
  drawTitle("Transmit > Saved", 70);
}

void listFavoriteSignals() {
  buttonCount = 0;
  tft.fillRect(0, 60, 240, 258, TFT_BLACK);
  drawHeaderFooter();
  drawTitle("Transmit > Favorites", 60);
}

void startSignalListen() {
  buttonCount = 0;
  listeningForSignal = true;
  signalCaptured = false;
  currentRawDataLen = 0;
  currentDecodedHex = 0;

  tft.fillRect(0, 60, 240, 258, TFT_BLACK);
  drawHeaderFooter();

  tft.setTextSize(2);
  tft.setTextColor(currentTheme.primary);
  tft.setCursor(60, 120);
  tft.println("Listening");
  tft.setCursor(50, 140);
  tft.println("for signal...");

  drawBackBtn(85, 200, 70, 40, []() {
    listeningForSignal = false;
    signalCaptured = false;
    signalOptions();
  });

  drawTitle("Receive > Listen", 70);
}

// ===================================================================================
// ============================== IR SIGNAL FUNCTIONS ================================
// ===================================================================================

void captureSignal() {
  // decode() már lefutott a loop()-ban, ne hívd újra
  if (IrReceiver.decodedIRData.rawlen == 0) {
    IrReceiver.resume();
    return;
  }

  currentRawDataLen = IrReceiver.decodedIRData.rawlen - 1;
  if (currentRawDataLen > 200) currentRawDataLen = 200;

  for (uint16_t i = 1; i <= currentRawDataLen; i++) {
    currentRawData[i - 1] = IrReceiver.irparams.rawbuf[i] * MICROS_PER_TICK;
  }

  currentDecodedHex = IrReceiver.decodedIRData.decodedRawData;
  signalCaptured = true;
  IrReceiver.resume();
}

void saveSignalToSD(const IRSignal &signal) {
  String filename = "/saved-signals/" + String(signal.name) + ".bin";

  File file = SD.open(filename.c_str(), FILE_WRITE);
  if (file) {
    file.write((uint8_t *)&signal, sizeof(IRSignal));
    file.close();
  }
}

void transmitSignal(const IRSignal &signal) {
  IrSender.sendRaw(signal.rawData, signal.rawDataLen, 38);
}

// Pronto Hex -> nyers mikroszekundumos tömb konverzió
// pronto[0]=formátum, pronto[1]=carrier freq kód, pronto[2]=seq1 burst-pár szám,
// pronto[3]=seq2 burst-pár szám, pronto[4..]=tényleges impulzusszám-páros adat
uint16_t prontoToRawSignal(const uint16_t *pronto, uint16_t *outRaw, uint8_t maxOut) {
  float unit = pronto[1] * 0.241246f;  // µs / Pronto time unit

  uint16_t totalPairs = pronto[2] + pronto[3];
  uint16_t totalValues = totalPairs * 2;
  if (totalValues > maxOut) totalValues = maxOut;
  if (totalValues > 150 - 4) totalValues = 150 - 4;  // codeArray[150] méret-védelem

  for (uint16_t i = 0; i < totalValues; i++) {
    outRaw[i] = (uint16_t)(pronto[4 + i] * unit);
  }
  return totalValues;
}

void deleteSignalFromSD(const char *filename) {
  String filepath = "/saved-signals/" + String(filename);
  SD.remove(filepath.c_str());
}

// ===================================================================================
// =============================== KEYBOARD FUNCTIONS ================================
// ===================================================================================

void drawKeyboard() {
  onKeyboard = true;
  buttonCount = 0;
  tft.fillRect(0, 60, 240, 258, TFT_BLACK);
  drawHeaderFooter();

  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(5, 70);
  tft.print("Save as: ");
  tft.setTextColor(currentTheme.primary);
  tft.setCursor(5, 80);
  tft.print(outputText);

  tft.setTextColor(currentTheme.secondary);
  tft.setCursor(5, 90);
  if (currentDecodedHex != 0) {
    tft.printf("HEX: 0x%lX", (unsigned long)currentDecodedHex);
  } else {
    tft.print("HEX: N/A (raw)");
  }

  const int cellWidth = 32;
  const int cellHeight = 32;
  const int startX = 8;
  const int startY = 105;

  // 26 betű, 7 oszlop -> 4 sor (utolsó sor 5 elemmel: V W X Y Z)
  for (int i = 0; i < 26; i++) {
    int row = i / 7;
    int col = i % 7;
    createTouchBox(startX + col * cellWidth, startY + row * cellHeight,
                   cellWidth, cellHeight,
                   currentTheme.primary, TFT_WHITE,
                   alphabet[i], keyboardButtonPressed);
  }

  // Kontroll sor: kötőjel / backspace / mentés
  int controlsY = startY + 4 * cellHeight;
  int controlWidth = 70;
  int gap = 6;

  createTouchBox(startX, controlsY, controlWidth, cellHeight,
                 currentTheme.primary, TFT_WHITE, "-", keyboardButtonPressed);

  createTouchBox(startX + controlWidth + gap, controlsY, controlWidth, cellHeight,
                 0xF800, TFT_WHITE, "<", keyboardButtonPressed);

  createTouchBox(startX + 2 * (controlWidth + gap), controlsY, controlWidth, cellHeight,
                 0x07E0, TFT_WHITE, ">", keyboardButtonPressed);

  int backBtnY = controlsY + cellHeight + 10;
  int backBtnWidth = 80;
  int backBtnHeight = 25;
  int backBtnX = (240 - backBtnWidth) / 2;

  createTouchBox(
    backBtnX, backBtnY, backBtnWidth, backBtnHeight,
    currentTheme.secondary, currentTheme.secondary,
    "Back",
    []() {
      outputText[0] = '\0';
      signalCaptured = false;
      onKeyboard = false;
      buttonCount = 0;
      signalOptions();
    },
    true);

  drawTitle("Enter signal name", 65);
}

void keyboardButtonPressed() {
  TouchButton *btn = &buttons[activeBtnIndex];
  const char *label = btn->label;

  if (strcmp(label, "<") == 0) {
    int len = strlen(outputText);
    if (len > 0) outputText[len - 1] = '\0';

  } else if (strcmp(label, ">") == 0) {
    if (strlen(outputText) > 0) {
      IRSignal signal;
      strncpy(signal.name, outputText, MAX_SAVED_SIGNAL_CHARS);
      signal.name[MAX_SAVED_SIGNAL_CHARS] = '\0';
      memcpy(signal.rawData, currentRawData, sizeof(signal.rawData));
      signal.rawDataLen = currentRawDataLen;
      saveSignalToSD(signal);

      tft.fillRect(0, 60, 240, 258, TFT_BLACK);
      printCentered("Saved!", 150, currentTheme.primary, 2);
      delay(2000);

      outputText[0] = '\0';
      signalCaptured = false;
      onKeyboard = false;
      buttonCount = 0;
      signalOptions();
      return;
    }

  } else {
    int len = strlen(outputText);
    if (len < MAX_SAVED_SIGNAL_CHARS) {
      outputText[len] = label[0];
      outputText[len + 1] = '\0';
    }
  }

  tft.fillRect(5, 80, 230, 8, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(currentTheme.primary);
  tft.setCursor(5, 80);
  tft.print(outputText);
}

// FIXED: Removed needsFullRedraw optimization
void listBuiltInSignals() {
  buttonCount = 0;
  onBuiltInSignals = true;
  onBuiltInBrands = false;

  tft.fillRect(0, 60, 240, 258, TFT_BLACK);
  drawHeaderFooter();

  if (currentBrandCodes == nullptr || currentBrandCodesLength == 0) {
    printCentered("Brand not", 120, currentTheme.primary, 2);
    printCentered("found!", 140, currentTheme.primary, 2);
    drawBackBtn(60, 200, 120, 40, builtInSignalsBrowser);
    drawTitle("Brand signals", 75);
    return;
  }

  const IRCode *brandCodes = currentBrandCodes;
  builtInSignalCount = currentBrandCodesLength;

  if (builtInSignalCount == 0) {
    tft.setTextSize(2);
    tft.setTextColor(currentTheme.primary);
    tft.setCursor(30, 120);
    tft.println("No signals");
    tft.setCursor(50, 140);
    tft.println("in brand!");

    drawBackBtn(60, 200, 120, 40, builtInSignalsBrowser);
    drawTitle("Brand signals", 75);
    return;
  }

  int maxVisible = 5;
  int yPos = 70;
  int lineHeight = 30;

  for (int i = builtInSignalScrollOffset; i < min(builtInSignalScrollOffset + maxVisible, builtInSignalCount); i++) {
    int highlightY = yPos;
    int highlightHeight = 25;

    if (i == builtInSignalHighlightedIndex) {
      tft.fillRect(5, highlightY, 230, highlightHeight, currentTheme.primary);
      tft.setTextColor(TFT_BLACK);
    } else {
      tft.setTextColor(TFT_WHITE);
    }

    tft.setTextSize(2);
    tft.setCursor(10, yPos + 5);
    tft.println(brandCodes[i].codeName);
    yPos += lineHeight;
  }

  if (builtInSignalScrollOffset > 0) {
    tft.fillTriangle(225, 85, 220, 90, 230, 90, TFT_WHITE);
  }
  if (builtInSignalScrollOffset + maxVisible < builtInSignalCount) {
    tft.fillTriangle(225, 225, 220, 220, 230, 220, TFT_WHITE);
  }

  createTouchBox(10, 235, 70, 30, currentTheme.secondary, currentTheme.secondary, "Up", []() {
    if (builtInSignalHighlightedIndex > 0) {
      builtInSignalHighlightedIndex--;
      if (builtInSignalHighlightedIndex < builtInSignalScrollOffset) {
        builtInSignalScrollOffset = builtInSignalHighlightedIndex;
      }
      listBuiltInSignals();
    }
  }, false, true);

  // FIXED: Changed builtInBrandScrollOffset to builtInSignalScrollOffset
  createTouchBox(85, 235, 70, 30, currentTheme.secondary, currentTheme.secondary, "Down", []() {
    if (builtInSignalHighlightedIndex < builtInSignalCount - 1) {
      builtInSignalHighlightedIndex++;
      if (builtInSignalHighlightedIndex >= builtInSignalScrollOffset + 5) {
        builtInSignalScrollOffset = builtInSignalHighlightedIndex - 5 + 1;
      }
      listBuiltInSignals();
    }
  }, false, true);

  // listBuiltInSignals() "Send" gombja:
  createTouchBox(160, 235, 70, 30, currentTheme.primary, currentTheme.primary, "Send", []() {
    const IRCode *selectedCode = &currentBrandCodes[builtInSignalHighlightedIndex];
    IRSignal signal;
    strncpy(signal.name, selectedCode->codeName, sizeof(signal.name) - 1);
    signal.name[sizeof(signal.name) - 1] = '\0';
    signal.rawDataLen = (uint8_t)prontoToRawSignal(selectedCode->codeArray, signal.rawData, 200);
    transmitSignal(signal);
  });

  tft.fillRect(10, 270, 220, 25, TFT_BLACK);
  createTouchBox(
    10, 270, 220, 25, currentTheme.secondary, currentTheme.secondary, "Back", []() {
      onBuiltInSignals = false;
      builtInSignalsBrowser();
    },
    true);

  String title = currentBrandPath + " signals";
  drawTitle(title.c_str(), 70);
}

// FIXED: Removed needsFullRedraw optimization
void builtInSignalsBrowser() {
  buttonCount = 0;
  onBuiltInBrands = true;
  builtInBrandCount = 0;

  tft.fillRect(0, 60, 240, 258, TFT_BLACK);
  drawHeaderFooter();

  builtInBrandCount = hardcodedBrandsLength;
  for (int i = 0; i < hardcodedBrandsLength; i++) {
    builtInBrands[i] = hardcodedBrands[i].brandName;
  }

  if (builtInBrandCount == 0) {
    tft.setTextSize(2);
    tft.setTextColor(currentTheme.primary);
    tft.setCursor(30, 120);
    tft.println("No brands");
    tft.setCursor(50, 140);
    tft.println("found!");

    drawBackBtn(60, 200, 120, 40, drawMenuUI);
    drawTitle("Built-in signals", 70);
    return;
  }

  int maxVisible = 5;
  int yPos = 70;
  int lineHeight = 30;

  for (int i = builtInBrandScrollOffset; i < min(builtInBrandScrollOffset + maxVisible, builtInBrandCount); i++) {
    int highlightY = yPos;
    int highlightHeight = 25;

    if (i == builtInBrandHighlightedIndex) {
      tft.fillRect(5, highlightY, 230, highlightHeight, currentTheme.primary);
      tft.setTextColor(TFT_BLACK);
    } else {
      tft.setTextColor(TFT_WHITE);
    }

    tft.setTextSize(2);
    tft.setCursor(10, yPos + 5);
    tft.println(builtInBrands[i]);
    yPos += lineHeight;
  }

  if (builtInBrandScrollOffset > 0) {
    tft.fillTriangle(225, 85, 220, 90, 230, 90, TFT_WHITE);
  }
  if (builtInBrandScrollOffset + maxVisible < builtInBrandCount) {
    tft.fillTriangle(225, 225, 220, 220, 230, 220, TFT_WHITE);
  }

  createTouchBox(10, 235, 70, 30, currentTheme.secondary, currentTheme.secondary, "Up", []() {
    if (builtInBrandHighlightedIndex > 0) {
      builtInBrandHighlightedIndex--;
      if (builtInBrandHighlightedIndex < builtInBrandScrollOffset) {
        builtInBrandScrollOffset = builtInBrandHighlightedIndex;
      }
      builtInSignalsBrowser();
    }
  }, false, true);

  createTouchBox(85, 235, 70, 30, currentTheme.secondary, currentTheme.secondary, "Down", []() {
    if (builtInBrandHighlightedIndex < builtInBrandCount - 1) {
      builtInBrandHighlightedIndex++;
      if (builtInBrandHighlightedIndex >= builtInBrandScrollOffset + 5) {
        builtInBrandScrollOffset = builtInBrandHighlightedIndex - 5 + 1;
      }
      builtInSignalsBrowser();
    }
  }, false, true);

  createTouchBox(160, 235, 70, 30, currentTheme.primary, currentTheme.primary, "Open", []() {
    int idx = builtInBrandHighlightedIndex;
    currentBrandCodes = hardcodedBrands[idx].codes;
    currentBrandCodesLength = hardcodedBrands[idx].codesLength;
    currentBrandPath = builtInBrands[idx];
    builtInSignalHighlightedIndex = 0;
    builtInSignalScrollOffset = 0;
    listBuiltInSignals();
  });

  tft.fillRect(10, 270, 220, 25, TFT_BLACK);
  createTouchBox(10, 270, 220, 25, currentTheme.secondary, currentTheme.secondary, "Back", drawMenuUI, true);

  drawTitle("Built-in signals", 70);
}

void sdData() {
  createOptions(SD_CARD_OPTIONS, 4, 10, 70);
  drawTitle("SD Card options", 75);
}

void showFileOptions() {
  buttonCount = 0;
  tft.fillRect(0, 60, 240, 258, TFT_BLACK);
  drawHeaderFooter();

  String selectedFile = sdFiles[sdHighlightedIndex];

  String actualFileName = selectedFile;
  if (!selectedFile.startsWith("[DIR] ")) {
    int parenIndex = actualFileName.lastIndexOf('(');
    if (parenIndex > 0) {
      actualFileName = actualFileName.substring(0, parenIndex - 1);
    }
  } else {
    actualFileName = actualFileName.substring(6);
  }

  tft.setTextSize(2);
  tft.setTextColor(currentTheme.primary);
  tft.setCursor(10, 80);
  tft.print("Selected:");
  tft.setCursor(10, 105);

  if (actualFileName.length() > 20) {
    actualFileName = actualFileName.substring(0, 17) + "...";
  }
  tft.println(actualFileName);

  if (!selectedFile.startsWith("[DIR] ")) {
    createTouchBox(30, 140, 180, 30, currentTheme.primary, currentTheme.primary, "Delete File", []() {
      Serial.println("Delete file functionality to be implemented");
    });

    createTouchBox(30, 180, 180, 30, currentTheme.primary, currentTheme.primary, "Rename File", []() {
      Serial.println("Rename file functionality to be implemented");
    });
  }

  createTouchBox(60, 240, 120, 30, currentTheme.secondary, currentTheme.secondary, "Back", drawSDFileBrowser, true);

  drawTitle("File Options", 85);
}

void listSDInfo() {
  buttonCount = 0;
  tft.fillRect(0, 60, 240, 258, TFT_BLACK);
  drawHeaderFooter();
  drawTitle("SD Card > Info", 75);

  uint64_t totalBytes = SD.totalBytes();
  uint64_t usedBytes = SD.usedBytes();
  uint64_t freeBytes = totalBytes - usedBytes;
  int savedSignalsCount = countFilesInDirectory("/saved-signals");

  int yPos = 110;
  tft.setTextSize(3);
  tft.setTextColor(currentTheme.primary);
  tft.setCursor(5, 72);
  tft.println("Storage");
  tft.drawFastHLine(0, yPos - 12, 240, currentTheme.primary);
  tft.drawFastHLine(0, yPos - 10, 240, currentTheme.primary);

  tft.setTextSize(2);
  tft.setCursor(10, yPos);
  tft.print("Full: ");
  tft.println(formatBytes(totalBytes));
  yPos += 20;
  tft.setCursor(10, yPos);
  tft.print("Free: ");
  tft.println(formatBytes(freeBytes));
  yPos += 20;
  tft.setCursor(10, yPos);
  tft.print("Used: ");
  tft.println(formatBytes(usedBytes));
  yPos += 40;

  tft.setTextSize(3);
  tft.setCursor(5, yPos);
  tft.println("Signals");
  tft.drawFastHLine(0, yPos + 26, 240, currentTheme.primary);
  tft.drawFastHLine(0, yPos + 28, 240, currentTheme.primary);
  yPos += 40;

  tft.setTextSize(2);
  tft.setCursor(10, yPos);
  tft.print("Saved: ");
  tft.println(savedSignalsCount);

  drawBackBtn(185, 130, 55, 50, sdData);
}

String formatBytes(uint64_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + " B";
  } else if (bytes < 1024 * 1024) {
    return String(bytes / 1024.0, 1) + " KB";
  } else if (bytes < 1024 * 1024 * 1024) {
    return String(bytes / (1024.0 * 1024.0), 1) + " MB";
  } else {
    return String(bytes / (1024.0 * 1024.0 * 1024.0), 1) + " GB";
  }
}

int countFilesInDirectory(const char *directoryPath) {
  File dir = SD.open(directoryPath);
  int count = 0;

  if (!dir) {
    return 0;
  }

  if (!dir.isDirectory()) {
    dir.close();
    return 0;
  }

  File entry = dir.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      count++;
    }
    entry.close();
    entry = dir.openNextFile();
  }

  dir.close();
  return count;
}

void listSDFiles() {
  buttonCount = 0;
  onSDFiles = true;
  sdFileCount = 0;
  sdHighlightedIndex = 0;
  sdScrollOffset = 0;
  currentPath = "/";

  loadSDFiles(currentPath);
  drawSDFileBrowser();
}

void loadSDFiles(String path) {
  sdFileCount = 0;

  File dir = SD.open(path);
  if (!dir) {
    Serial.println("Failed to open directory");
    return;
  }

  File entry = dir.openNextFile();
  while (entry && sdFileCount < 50) {
    String fileName = String(entry.name());

    if (fileName.startsWith("/")) {
      fileName = fileName.substring(1);
    }

    if (entry.isDirectory()) {
      sdFiles[sdFileCount] = "DIR - " + fileName;
    } else {
      String sizeStr = formatBytes(entry.size());
      sdFiles[sdFileCount] = fileName + " - " + sizeStr;
    }

    sdFileCount++;
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
}

void drawSDFileBrowser() {
  buttonCount = 0;
  tft.fillRect(0, 60, 240, 258, TFT_BLACK);
  drawHeaderFooter();

  tft.setTextSize(1);
  tft.setTextColor(currentTheme.primary);
  tft.setCursor(5, 65);
  tft.print("Path: ");
  tft.println(currentPath);

  if (sdFileCount == 0) {
    tft.setTextSize(2);
    tft.setTextColor(currentTheme.primary);
    tft.setCursor(30, 120);
    tft.println("No files");
    tft.setCursor(50, 140);
    tft.println("found!");

    drawBackBtn(60, 180, 120, 40, sdData);
    drawTitle("SD Card > Files", 75);
    return;
  }

  int yPos = 80;
  int lineHeight = 25;

  for (int i = sdScrollOffset; i < min(sdScrollOffset + sdMaxVisible, sdFileCount); i++) {
    int highlightY = yPos;
    int highlightHeight = 22;

    if (i == sdHighlightedIndex) {
      tft.fillRect(5, highlightY, 230, highlightHeight, currentTheme.primary);
      tft.setTextColor(TFT_BLACK);
    } else {
      tft.fillRect(5, highlightY, 230, highlightHeight, TFT_BLACK);
      tft.setTextColor(TFT_WHITE);
    }

    tft.setTextSize(1);
    tft.setCursor(10, yPos + 5);

    String displayName = sdFiles[i];
    if (displayName.length() > 35) {
      displayName = displayName.substring(0, 32) + "...";
    }
    tft.println(displayName);

    yPos += lineHeight;
  }

  if (sdScrollOffset > 0) {
    tft.fillTriangle(225, 85, 220, 90, 230, 90, TFT_WHITE);
  }
  if (sdScrollOffset + sdMaxVisible < sdFileCount) {
    tft.fillTriangle(225, 220, 220, 215, 230, 215, TFT_WHITE);
  }

  createTouchBox(10, 240, 70, 30, currentTheme.secondary, currentTheme.secondary, "Up", []() {
    if (sdHighlightedIndex > 0) {
      sdHighlightedIndex--;
      if (sdHighlightedIndex < sdScrollOffset) {
        sdScrollOffset = sdHighlightedIndex;
      }
      drawSDFileBrowser();
    }
  }, false, true);

  createTouchBox(85, 240, 70, 30, currentTheme.secondary, currentTheme.secondary, "Down", []() {
    if (sdHighlightedIndex < sdFileCount - 1) {
      sdHighlightedIndex++;
      if (sdHighlightedIndex >= sdScrollOffset + sdMaxVisible) {
        sdScrollOffset = sdHighlightedIndex - sdMaxVisible + 1;
      }
      drawSDFileBrowser();
    }
  }, false, true);

  String selectedFile = sdFiles[sdHighlightedIndex];
  bool isDirectory = selectedFile.startsWith("DIR - ");

  if (isDirectory) {
    createTouchBox(160, 240, 70, 30, currentTheme.primary, currentTheme.primary, "Open", []() {
      String selectedFile = sdFiles[sdHighlightedIndex];
      String dirName = selectedFile.substring(6);

      if (currentPath == "/") {
        currentPath = "/" + dirName;
      } else {
        currentPath = currentPath + "/" + dirName;
      }

      loadSDFiles(currentPath);
      sdHighlightedIndex = 0;
      sdScrollOffset = 0;
      drawSDFileBrowser();
    });
  } else {
    createTouchBox(160, 240, 70, 30, 0xF800, TFT_WHITE, "Del", deleteSelectedFile);
  }

  const char *backLabel = "Back";
  void (*backCallback)() = sdData;

  if (currentPath != "/") {
    backLabel = "Up Dir";
    backCallback = []() {
      int lastSlash = currentPath.lastIndexOf('/');
      if (lastSlash > 0) {
        currentPath = currentPath.substring(0, lastSlash);
      } else {
        currentPath = "/";
      }

      loadSDFiles(currentPath);
      sdHighlightedIndex = 0;
      sdScrollOffset = 0;
      drawSDFileBrowser();
    };
  }

  createTouchBox(60, 275, 120, 25, currentTheme.secondary, currentTheme.secondary, backLabel, backCallback, true);

  drawTitle("SD Card > Files", 75);
}

void sdFormatOptions() {
  buttonCount = 0;
  tft.fillRect(0, 60, 240, 258, TFT_BLACK);
  drawHeaderFooter();
  createOptions(SD_FORMAT_OPTIONS, 2, 10, 100);
  drawTitle("SD Card > Format", 75);
}

bool deleteDirectory(const char *path) {
  File dir = SD.open(path);

  if (!dir) {
    return false;
  }

  if (!dir.isDirectory()) {
    dir.close();
    return SD.remove(path);
  }

  File entry = dir.openNextFile();
  while (entry) {
    String entryPath = String(path) + "/" + String(entry.name());

    if (entry.isDirectory()) {
      entry.close();
      if (!deleteDirectory(entryPath.c_str())) {
        dir.close();
        return false;
      }
    } else {
      entry.close();
      if (!SD.remove(entryPath.c_str())) {
        dir.close();
        return false;
      }
    }

    entry = dir.openNextFile();
  }

  dir.close();
  return SD.rmdir(path);
}

void formatSD() {
  File root = SD.open("/");

  if (!root) {
    return;
  }

  tft.setTextColor(currentTheme.primary);
  tft.setTextSize(2);
  tft.setCursor(30, 100);
  tft.println("Formatting...");

  File entry = root.openNextFile();
  while (entry) {
    tft.fillRect(0, 130, 240, 50, TFT_BLACK);
    String entryName = String(entry.name());
    bool isDir = entry.isDirectory();
    entry.close();
    tft.fillRect(0, 60, 240, 258, TFT_BLACK);

    if (entryName == "System Volume Information" || entryName == "built-in-signals") {
      tft.setCursor(10, 130);
      tft.setTextColor(currentTheme.primary);
      tft.print("Skipping:");
      tft.setCursor(0, 160);
      tft.setTextColor(TFT_WHITE);
      tft.println(entryName);
      entry = root.openNextFile();
      delay(1000);
      continue;
    }

    String fullPath = "/" + entryName;

    tft.setCursor(10, 130);
    tft.setTextColor(currentTheme.primary);
    tft.print("Deleting:");
    tft.setCursor(0, 160);
    tft.setTextColor(TFT_WHITE);
    tft.println(entryName);

    bool success;
    if (isDir) {
      success = deleteDirectory(fullPath.c_str());
    } else {
      success = SD.remove(fullPath.c_str());
    }

    if (success) {
      tft.setCursor(10, 130);
      tft.setTextColor(0x07E0);
      tft.println("Deleted:");
      tft.setCursor(0, 160);
      tft.setTextColor(TFT_WHITE);
      tft.println(entryName);
      delay(1000);
    } else {
      tft.setCursor(10, 130);
      tft.setTextColor(0xF800);
      tft.println("Failed:");
      tft.setCursor(0, 160);
      tft.setTextColor(TFT_WHITE);
      tft.println(entryName);
      delay(1000);
    }

    entry = root.openNextFile();
  }

  root.close();

  tft.fillRect(0, 60, 240, 258, TFT_BLACK);
  tft.setTextColor(0x07E0);
  tft.setTextSize(2);
  tft.setCursor(20, 140);
  tft.println("Formatting done!");

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(30, 170);
  tft.println("Preserved: built-in-signals");

  delay(2000);

  if (!SD.exists("/saved-signals")) {
    SD.mkdir("/saved-signals");
  }

  drawMenuUI();
}

String extractPrefix(String filename) {
  filename.replace(".bin", "");

  int hyphenIndex = filename.indexOf('-');

  if (hyphenIndex > 0) {
    return filename.substring(0, hyphenIndex);
  }

  return filename;
}

// FIXED: Removed needsFullRedraw optimization and added SPI management
void drawGroupedSignalsList() {
  buttonCount = 0;
  tft.fillRect(0, 60, 240, 258, TFT_BLACK);
  drawHeaderFooter();

  if (groupedSignalCount == 0) {
    printCentered("No signals", 120, currentTheme.primary, 2);
    printCentered("in group!", 140, currentTheme.primary, 2);
    drawBackBtn(60, 200, 120, 40, listSavedSignals);
    drawTitle("Group signals", 75);
    return;
  }

  int maxVisible = 5, yPos = 70, lineHeight = 30;
  for (int i = groupedSignalScrollOffset; i < min(groupedSignalScrollOffset + maxVisible, groupedSignalCount); i++) {
    if (i == groupedSignalHighlightedIndex) {
      tft.fillRect(5, yPos, 230, 25, currentTheme.primary);
      tft.setTextColor(TFT_BLACK);
    } else {
      tft.setTextColor(TFT_WHITE);
    }
    tft.setTextSize(2);
    tft.setCursor(10, yPos + 5);
    String displayName = groupedSignalFiles[i];
    displayName.replace(".bin", "");
    int hyphenIndex = displayName.indexOf('-');
    if (hyphenIndex >= 0) displayName = displayName.substring(hyphenIndex + 1);
    tft.println(displayName);
    yPos += lineHeight;
  }

  if (groupedSignalScrollOffset > 0)
    tft.fillTriangle(225, 85, 220, 90, 230, 90, TFT_WHITE);
  if (groupedSignalScrollOffset + maxVisible < groupedSignalCount)
    tft.fillTriangle(225, 225, 220, 220, 230, 220, TFT_WHITE);

  createTouchBox(
    10, 235, 70, 30, currentTheme.secondary, currentTheme.secondary, "Up", []() {
      if (groupedSignalHighlightedIndex > 0) {
        groupedSignalHighlightedIndex--;
        if (groupedSignalHighlightedIndex < groupedSignalScrollOffset)
          groupedSignalScrollOffset = groupedSignalHighlightedIndex;
        drawGroupedSignalsList();
      }
    },
    false, true);

  createTouchBox(
    85, 235, 70, 30, currentTheme.secondary, currentTheme.secondary, "Down", []() {
      if (groupedSignalHighlightedIndex < groupedSignalCount - 1) {
        groupedSignalHighlightedIndex++;
        if (groupedSignalHighlightedIndex >= groupedSignalScrollOffset + 5)
          groupedSignalScrollOffset = groupedSignalHighlightedIndex - 5 + 1;
        drawGroupedSignalsList();
      }
    },
    false, true);

  createTouchBox(160, 235, 70, 30, currentTheme.primary, currentTheme.primary, "Send", []() {
    String filepath = "/saved-signals/" + groupedSignalFiles[groupedSignalHighlightedIndex];
    File file = SD.open(filepath.c_str(), FILE_READ);
    if (file) {
      IRSignal signal;
      file.read((uint8_t *)&signal, sizeof(IRSignal));
      file.close();
      digitalWrite(SD_CS, HIGH);
      digitalWrite(TOUCH_CS, HIGH);
      delay(10);
      transmitSignal(signal);
    }
  });

  tft.fillRect(10, 270, 220, 25, TFT_BLACK);
  createTouchBox(
    10, 270, 220, 25, currentTheme.secondary, currentTheme.secondary, "Back", []() {
      onSavedSignals = false;
      listSavedSignals();
    },
    true);

  String title = currentSavedGroup + " signals";
  drawTitle(title.c_str(), 70);
}

void listGroupedSignals() {
  onSavedSignals = true;
  onSavedSignalGroups = false;
  groupedSignalCount = 0;

  File dir = SD.open("/saved-signals");
  if (dir) {
    File entry = dir.openNextFile();
    while (entry && groupedSignalCount < 50) {
      if (!entry.isDirectory()) {
        String filename = String(entry.name());
        if (extractPrefix(filename) == currentSavedGroup) {
          groupedSignalFiles[groupedSignalCount++] = filename;
        }
      }
      entry.close();
      entry = dir.openNextFile();
    }
    dir.close();
  }

  groupedSignalHighlightedIndex = 0;
  groupedSignalScrollOffset = 0;
  drawGroupedSignalsList();
}

void deleteSelectedFile() {
  String selectedFile = sdFiles[sdHighlightedIndex];

  if (!selectedFile.startsWith("DIR - ")) {
    String actualFileName = selectedFile;
    int dashIndex = actualFileName.lastIndexOf(" - ");
    if (dashIndex > 0) {
      actualFileName = actualFileName.substring(0, dashIndex);
    }

    String fullPath;
    if (currentPath == "/") {
      fullPath = "/" + actualFileName;
    } else {
      fullPath = currentPath + "/" + actualFileName;
    }

    bool success = SD.remove(fullPath.c_str());

    if (success) {
      loadSDFiles(currentPath);

      if (sdHighlightedIndex >= sdFileCount && sdFileCount > 0) {
        sdHighlightedIndex = sdFileCount - 1;
      }
      if (sdScrollOffset > sdHighlightedIndex) {
        sdScrollOffset = max(0, sdHighlightedIndex);
      }

      drawSDFileBrowser();
    } else {
      tft.fillRect(0, 60, 240, 100, TFT_BLACK);
      tft.setTextSize(2);
      tft.setTextColor(0xF800);
      tft.setCursor(20, 100);
      tft.println("Delete");
      tft.setCursor(20, 120);
      tft.println("failed!");
      delay(1500);
      drawSDFileBrowser();
    }
  }
}

// ===================================================================================
// ===================================== THEMES ======================================
// ===================================================================================

void themeOptions() {
  createOptions(THEME_OPTIONS, 4, 10, 70);
  drawTitle("Change theme", 85);
}

void setThemeFuturisticRed() {
  setTheme(0);
}

void setThemeFuturisticGreen() {
  setTheme(1);
}

void setThemeFuturisticPurple() {
  setTheme(2);
}

void setTheme(uint8_t themeIndex) {
  switch (themeIndex) {
    case 0:
      currentTheme = THEME_FUTURISTIC_RED;
      break;
    case 1:
      currentTheme = THEME_FUTURISTIC_GREEN;
      break;
    case 2:
      currentTheme = THEME_FUTURISTIC_PURPLE;
      break;
    default:
      currentTheme = THEME_FUTURISTIC_RED;
      break;
  }

  drawMenuUI();
}

// ===================================================================================
// ============================== TOUCH BUTTONS ======================================
// ===================================================================================

int processTouchButtons(int tx, int ty) {
  for (int i = 0; i < buttonCount; i++) {
    TouchButton *btn = &buttons[i];
    if (isTouchInButton(btn, tx, ty)) {
      btn->pressed = true;
      drawButton(btn, true);
      activeBtnIndex = i;
      if (btn->callback != NULL) {
        btn->callback();
      }
      return i;
    }
  }
  return -1;
}

void createTouchBox(int x, int y, int width, int height, uint16_t color, uint16_t textColor, const char *label, void (*callback)(), bool isBackButton, bool repeatable) {
  TouchButton *btn = &buttons[buttonCount];
  btn->x = x;
  btn->y = y;
  btn->w = width;
  btn->h = height;
  btn->color = color;
  btn->textColor = textColor;
  btn->label = label;
  btn->callback = callback;
  btn->pressed = false;
  btn->isBackButton = isBackButton;
  btn->repeatable = repeatable;

  drawButton(btn, false);
  buttonCount++;
}

bool isTouchInButton(TouchButton *btn, int tx, int ty) {
  return (tx >= btn->x && tx <= (btn->x + btn->w) && ty >= btn->y && ty <= (btn->y + btn->h));
}

void drawButton(TouchButton *btn, bool active) {
  if (btn->isBackButton) active = !active;

  tft.startWrite();

  tft.fillRect(btn->x, btn->y, btn->w, btn->h, TFT_BLACK);

  if (active) {
    tft.fillRect(btn->x, btn->y, btn->w, btn->h, btn->color);
    tft.drawRect(btn->x, btn->y, btn->w, btn->h, TFT_WHITE);

    const int cs = 8;
    tft.drawFastHLine(btn->x, btn->y, cs, TFT_WHITE);
    tft.drawFastVLine(btn->x, btn->y, cs, TFT_WHITE);
    tft.drawFastHLine(btn->x + btn->w - cs, btn->y, cs, TFT_WHITE);
    tft.drawFastVLine(btn->x + btn->w - 1, btn->y, cs, TFT_WHITE);
    tft.drawFastHLine(btn->x, btn->y + btn->h - 1, cs, TFT_WHITE);
    tft.drawFastVLine(btn->x, btn->y + btn->h - cs, cs, TFT_WHITE);
    tft.drawFastHLine(btn->x + btn->w - cs, btn->y + btn->h - 1, cs, TFT_WHITE);
    tft.drawFastVLine(btn->x + btn->w - 1, btn->y + btn->h - cs, cs, TFT_WHITE);
  } else {
    tft.drawRect(btn->x - 1, btn->y - 1, btn->w + 2, btn->h + 2, currentTheme.accent);
    tft.drawRect(btn->x - 2, btn->y - 2, btn->w + 4, btn->h + 4, currentTheme.darkest);
    tft.fillRect(btn->x, btn->y, btn->w, btn->h, TFT_BLACK);
    tft.drawRect(btn->x, btn->y, btn->w, btn->h, btn->color);

    const int cs = 8;
    tft.drawFastHLine(btn->x, btn->y, cs, btn->color);
    tft.drawFastVLine(btn->x, btn->y, cs, btn->color);
    tft.drawFastHLine(btn->x + btn->w - cs, btn->y, cs, btn->color);
    tft.drawFastVLine(btn->x + btn->w - 1, btn->y, cs, btn->color);
    tft.drawFastHLine(btn->x, btn->y + btn->h - 1, cs, btn->color);
    tft.drawFastVLine(btn->x, btn->y + btn->h - cs, cs, btn->color);
    tft.drawFastHLine(btn->x + btn->w - cs, btn->y + btn->h - 1, cs, btn->color);
    tft.drawFastVLine(btn->x + btn->w - 1, btn->y + btn->h - cs, cs, btn->color);

    for (int j = 2; j < btn->h - 2; j += 5) {
      tft.drawFastHLine(btn->x + 2, btn->y + j, btn->w - 4, currentTheme.darkest);
    }
  }

  tft.setTextSize(2);
  int16_t tw = (int16_t)tft.textWidth(btn->label);
  int16_t th = (int16_t)tft.fontHeight();
  int16_t textX = btn->x + (btn->w - tw) / 2;
  int16_t textY = btn->y + (btn->h - th) / 2;

  tft.setTextColor(active ? TFT_BLACK : btn->color,
                   active ? btn->color : TFT_BLACK);
  tft.setCursor(textX, textY);
  tft.print(btn->label);

  tft.endWrite();
}

void createOptions(const Option options[], int count, int x, int y, int btnWidth, int btnHeight) {
  buttonCount = 0;
  tft.fillRect(0, 60, 240, 258, TFT_BLACK);

  for (int i = 0; i < count; i++) {
    bool isBack = (i == count - 1 && strcmp(options[i].name, "Back") == 0);
    createTouchBox(x, y + 15 + (btnHeight + 5) * i, btnWidth, btnHeight,
                   currentTheme.primary, currentTheme.primary,
                   options[i].name, options[i].callback, isBack);
  }

  drawHeaderFooter();
}

void createGridOptions(const Option options[], int count, int rows, int cols, int startX, int startY, int spacing, int btnSize) {
  int buttonIndex = 0;
  for (int row = 0; row < rows && buttonIndex < count; row++) {
    for (int col = 0; col < cols && buttonIndex < count; col++) {
      int x = startX + col * (btnSize + spacing);
      int y = startY + row * (btnSize + spacing);

      bool isBack = (buttonIndex == count - 1 && strcmp(options[buttonIndex].name, "Back") == 0);

      createTouchBox(x, y, btnSize, btnSize,
                     currentTheme.primary, currentTheme.primary,
                     options[buttonIndex].name, options[buttonIndex].callback, isBack);

      buttonIndex++;
    }
  }
}

// ===================================================================================
// ================================= IR Rx & Tx ======================================
// ===================================================================================

void sendFixSignal(const uint16_t *signalData, size_t length) {
  IrSender.sendRaw(signalData, length, 38);
}