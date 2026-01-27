#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include <IRremote.hpp>
#include "./IR-codes.h"

// ===================================================================================
// ==================================== PINS =========================================
// ===================================================================================

// Display
constexpr uint8_t TFT_CS = 7;
constexpr uint8_t TFT_RST = 10;
constexpr uint8_t TFT_DC = 2;
constexpr uint8_t TFT_MOSI = 6;
constexpr uint8_t TFT_MISO = 5;
constexpr uint8_t TFT_CLK = 4;

// Display > Touch
constexpr uint8_t TOUCH_CS = 8;
constexpr uint8_t TOUCH_IRQ = 9;

// Display > SD
constexpr uint8_t SD_CS = 3;

// IR Tx & Rx
constexpr uint8_t IR_TX = 0;
constexpr uint8_t IR_RX = 1;

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
void processTouchButtons(int tx, int ty);
void drawButton(TouchButton *btn, bool active);
void drawHeaderFooter();
uint16_t read16(File &f);
uint32_t read32(File &f);
void drawBMP(const char *filename, int16_t x, int16_t y);
void createTouchBox(int x, int y, int width, int height, uint16_t color, uint16_t textColor, const char *label, void (*callback)(), bool isBackButton = false);
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
SPIClass spi(FSPI);
Adafruit_ILI9341 tft = Adafruit_ILI9341(&spi, TFT_DC, TFT_CS, TFT_RST);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
ThemeColors currentTheme;

// Helper variables
TouchButton buttons[30];  // Max number of buttons as predefined array
uint8_t buttonCount = 0;
bool initializedSD = false;

// Default theme
const ThemeColors DEFAULT_THEME = THEME_FUTURISTIC_RED;

// Touch calibration
constexpr uint16_t TS_MINX = 240;
constexpr uint16_t TS_MINY = 250;
constexpr uint16_t TS_MAXX = 3850;
constexpr uint16_t TS_MAXY = 3750;

// Debouncing variables
unsigned long lastTouchTime = 0;
constexpr unsigned long DEBOUNCE_DELAY = 300;  // ms

// IR Signal variables
uint16_t currentRawData[200];
uint8_t currentRawDataLen = 0;
bool signalCaptured = false;
bool listeningForSignal = false;
int mark_excess_micros = 20;

// Keyboard variables
const char *const alphabet[29] = {
  "Q", "W", "E", "R", "T", "Z", "U", "I", "O", "P",
  "A", "S", "D", "F", "G", "H", "J", "K", "L",
  "Y", "X", "C", "V", "-", "B", "N", "M", "<", ">"
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

void loop() {
  // Handle signal listening
  if (listeningForSignal && !signalCaptured) {
    if (IrReceiver.decode()) {
      captureSignal();

      if (currentRawDataLen < 10) {
        buttonCount = 0;
        tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
        tft.setTextSize(2);
        tft.setTextColor(currentTheme.primary);
        tft.setCursor(30, 120);
        tft.println("Invalid");
        tft.setCursor(30, 140);
        tft.println("signal!");
        delay(2000);
        startSignalListen();
      } else {
        signalCaptured = true;
        listeningForSignal = false;
        tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
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

  if (touch.touched()) {
    TS_Point p = touch.getPoint();

    // Map to screen coordinates for portrait mode
    int x = map(p.x, TS_MINX, TS_MAXX, 240, 0);
    int y = map(p.y, TS_MINY, TS_MAXY, 320, 0);

    // Constrain
    x = constrain(x, 0, 239);
    y = constrain(y, 0, 319);

    // Process button touches
    processTouchButtons(x, y);

    delay(50);
  } else {
    // Reset all button states when no touch
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

  // Set theme
  currentTheme = DEFAULT_THEME;

  // IR Tx & Rx
  IrSender.begin(IR_TX);
  IrReceiver.begin(IR_RX);

  // SPI
  spi.begin(TFT_CLK, TFT_MISO, TFT_MOSI);
  pinMode(TFT_CS, OUTPUT);
  pinMode(TOUCH_CS, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);
  digitalWrite(SD_CS, HIGH);

  // Display
  tft.begin(27000000);
  tft.setRotation(2);

  // SD
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);

  if (!SD.begin(SD_CS, spi, 4000000)) {
    initializedSD = false;
  } else {
    initializedSD = true;
    // Create saved-signals directory if it doesn't exist
    if (!SD.exists("/saved-signals")) {
      SD.mkdir("/saved-signals");
    }
  }

  // Setup
  digitalWrite(SD_CS, HIGH);
  delay(10);

  // Draw UI and menu
  drawMenuUI();

  // Touch
  if (!touch.begin(spi)) {
    Serial.println("Touch init failed!");
  }
  touch.setRotation(2);
}

void drawTitle(const char *titleName, uint16_t x, uint16_t y) {
  tft.setTextSize(1);
  tft.setCursor(x, y);
  tft.setTextColor(currentTheme.primary);
  tft.println(titleName);
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
  tft.fillScreen(ILI9341_BLACK);

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
  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);

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
  buttonCount = 0;
  onSavedSignalGroups = true;
  onSavedSignals = false;
  savedSignalGroupCount = 0;

  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
  drawHeaderFooter();

  // Load signal files and extract unique prefixes
  File dir = SD.open("/saved-signals");
  if (!dir) {
    tft.setTextSize(2);
    tft.setTextColor(currentTheme.primary);
    tft.setCursor(20, 120);
    tft.println("No signals");
    tft.setCursor(40, 140);
    tft.println("found!");

    drawBackBtn(60, 200, 120, 40, signalOptions);
    drawTitle("Transmit > Saved", 70);
    return;
  }

  // Collect all unique prefixes
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

      if (!exists && savedSignalGroupCount < 50) {
        savedSignalGroups[savedSignalGroupCount] = prefix;
        savedSignalGroupCount++;
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();

  if (savedSignalGroupCount == 0) {
    tft.setTextSize(2);
    tft.setTextColor(currentTheme.primary);
    tft.setCursor(60, 150);
    tft.println("No signals");
    tft.setCursor(85, 170);
    tft.println("saved!");

    drawBackBtn(60, 200, 120, 40, signalOptions);
    drawTitle("Transmit > Saved", 70);
    return;
  }

  // Display groups
  int maxVisible = 5;
  int yPos = 70;
  int lineHeight = 30;

  for (int i = savedSignalGroupScrollOffset; i < min(savedSignalGroupScrollOffset + maxVisible, savedSignalGroupCount); i++) {
    int highlightY = yPos;
    int highlightHeight = 25;

    if (i == savedSignalGroupHighlightedIndex) {
      tft.fillRect(5, highlightY, 230, highlightHeight, currentTheme.primary);
      tft.setTextColor(ILI9341_BLACK);
    } else {
      tft.setTextColor(ILI9341_WHITE);
    }

    tft.setTextSize(2);
    tft.setCursor(10, yPos + 5);
    tft.println(savedSignalGroups[i]);
    yPos += lineHeight;
  }

  // Scroll indicators
  if (savedSignalGroupScrollOffset > 0) {
    tft.fillTriangle(225, 85, 220, 90, 230, 90, ILI9341_WHITE);
  }
  if (savedSignalGroupScrollOffset + maxVisible < savedSignalGroupCount) {
    tft.fillTriangle(225, 225, 220, 220, 230, 220, ILI9341_WHITE);
  }

  // Navigation buttons
  createTouchBox(10, 235, 70, 30, currentTheme.secondary, currentTheme.secondary, "Up", []() {
    if (savedSignalGroupHighlightedIndex > 0) {
      savedSignalGroupHighlightedIndex--;
      if (savedSignalGroupHighlightedIndex < savedSignalGroupScrollOffset) {
        savedSignalGroupScrollOffset = savedSignalGroupHighlightedIndex;
      }
      listSavedSignals();
    }
  });

  createTouchBox(85, 235, 70, 30, currentTheme.secondary, currentTheme.secondary, "Down", []() {
    if (savedSignalGroupHighlightedIndex < savedSignalGroupCount - 1) {
      savedSignalGroupHighlightedIndex++;
      if (savedSignalGroupHighlightedIndex >= savedSignalGroupScrollOffset + 5) {
        savedSignalGroupScrollOffset = savedSignalGroupHighlightedIndex - 5 + 1;
      }
      listSavedSignals();
    }
  });

  createTouchBox(160, 235, 70, 30, currentTheme.primary, currentTheme.primary, "Open", []() {
    currentSavedGroup = savedSignalGroups[savedSignalGroupHighlightedIndex];
    groupedSignalHighlightedIndex = 0;
    groupedSignalScrollOffset = 0;
    listGroupedSignals();
  });

  tft.fillRect(10, 270, 220, 25, ILI9341_BLACK);
  createTouchBox(10, 270, 220, 25, currentTheme.secondary, currentTheme.secondary, "Back", signalOptions, true);

  drawTitle("Transmit > Saved", 70);
}

void listFavoriteSignals() {
  buttonCount = 0;
  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
  drawHeaderFooter();
  drawTitle("Transmit > Favorites", 60);
}

void startSignalListen() {
  buttonCount = 0;
  listeningForSignal = true;
  signalCaptured = false;
  currentRawDataLen = 0;

  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
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
  if (!IrReceiver.decode()) return;

  if (IrReceiver.decodedIRData.rawlen == 0) {
    IrReceiver.resume();
    return;
  }

  currentRawDataLen = IrReceiver.decodedIRData.rawlen - 1;

  for (uint16_t i = 1; i < IrReceiver.decodedIRData.rawlen; i++) {
    uint32_t duration = IrReceiver.irparams.rawbuf[i] * MICROS_PER_TICK;
    currentRawData[i - 1] = duration;
  }

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
  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
  drawHeaderFooter();

  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(5, 70);
  tft.print("Save as: ");
  tft.setTextColor(currentTheme.primary);
  tft.setCursor(5, 80);
  tft.print(outputText);

  const int cellWidth = 32;
  const int cellHeight = 32;
  const int spaceWidth = cellWidth * 3;
  const int startX = 8;
  const int startY = 105;

  for (int i = 0; i < 29; i++) {
    const char *currentChar = alphabet[i];
    int currentCellWidth = cellWidth;
    uint16_t btnColor = currentTheme.primary;
    uint16_t btnTextColor = ILI9341_WHITE;
    int row, col;

    if (i >= 0 && i <= 6) {
      row = 0;
      col = i;
    } else if (i == 7) {
      row = 1;
      col = 0;
    } else if (i == 8) {
      row = 1;
      col = 1;
    } else if (i == 9) {
      row = 1;
      col = 2;
    } else if (i >= 10 && i <= 13) {
      row = 1;
      col = i - 7;
    } else if (i >= 14 && i <= 18) {
      row = 2;
      col = i - 14;
    } else if (i >= 19 && i <= 20) {
      row = 2;
      col = i - 14;
    } else if (i == 21) {
      row = 3;
      col = 1;
    } else if (i == 22) {
      row = 3;
      col = 2;
    } else if (i == 24) {
      row = 3;
      col = 3;
    } else if (i == 25) {
      row = 3;
      col = 4;
    } else if (i == 26) {
      row = 3;
      col = 5;
    } else if (i == 27) {
      row = 4;
      col = 1;
      btnColor = 0xF800;
      btnTextColor = ILI9341_WHITE;
    } else if (i == 23) {
      row = 4;
      col = 2;
      currentCellWidth = spaceWidth;
    } else if (i == 28) {
      row = 4;
      col = 5;
      btnColor = 0x07E0;
      btnTextColor = ILI9341_WHITE;
    } else continue;

    int xOffset = startX + col * cellWidth;
    int yOffset = startY + row * cellHeight;

    createTouchBox(xOffset, yOffset, currentCellWidth, cellHeight,
                   btnColor, btnTextColor,
                   currentChar,
                   keyboardButtonPressed);
  }

  int lastKeyboardRowY = startY + (4 * cellHeight);
  int backBtnY = lastKeyboardRowY + cellHeight + 10;
  int backBtnWidth = 80;
  int backBtnHeight = 25;
  int backBtnX = (240 - backBtnWidth) / 2;

  tft.fillRect(backBtnX, backBtnY, backBtnWidth, backBtnHeight, ILI9341_BLACK);

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
  Serial.println("=== KEYBOARD BUTTON PRESSED ===");
  Serial.print("activeBtnIndex: ");
  Serial.println(activeBtnIndex);
  Serial.print("Button count: ");
  Serial.println(buttonCount);

  if (activeBtnIndex < buttonCount) {
    Serial.print("Button label: ");
    Serial.println(buttons[activeBtnIndex].label);
  } else {
    Serial.println("ERROR: activeBtnIndex out of range!");
    return;
  }

  TouchButton *btn = &buttons[activeBtnIndex];

  int alphabetIndex = -1;
  for (int i = 0; i < 29; i++) {
    if (strcmp(btn->label, alphabet[i]) == 0) {
      alphabetIndex = i;
      break;
    }
  }

  if (alphabetIndex == -1) return;

  String selectedChar = alphabet[alphabetIndex];

  if (selectedChar == "<") {
    int len = strlen(outputText);
    if (len > 0) outputText[len - 1] = '\0';

    tft.fillRect(5, 80, 230, 20, ILI9341_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(currentTheme.primary);
    tft.setCursor(5, 80);
    tft.print(outputText);

  } else if (selectedChar == "_") {
    int len = strlen(outputText);
    if (len < MAX_SAVED_SIGNAL_CHARS) {
      outputText[len] = '-';
      outputText[len + 1] = '\0';

      tft.fillRect(5, 80, 230, 20, ILI9341_BLACK);
      tft.setTextSize(1);
      tft.setTextColor(currentTheme.primary);
      tft.setCursor(5, 80);
      tft.print(outputText);
    }

  } else if (selectedChar == ">") {
    if (strlen(outputText) > 0) {
      IRSignal signal;
      strncpy(signal.name, outputText, MAX_SAVED_SIGNAL_CHARS);
      signal.name[MAX_SAVED_SIGNAL_CHARS] = '\0';
      memcpy(signal.rawData, currentRawData, sizeof(signal.rawData));
      signal.rawDataLen = currentRawDataLen;
      saveSignalToSD(signal);

      tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
      tft.setTextSize(2);
      tft.setTextColor(currentTheme.primary);
      tft.setCursor(80, 150);
      tft.println("Saved!");
      delay(2000);

      outputText[0] = '\0';
      signalCaptured = false;
      onKeyboard = false;
      buttonCount = 0;
      signalOptions();
    }
  } else {
    int len = strlen(outputText);
    if (len < MAX_SAVED_SIGNAL_CHARS) {
      outputText[len] = selectedChar[0];
      outputText[len + 1] = '\0';

      tft.fillRect(5, 80, 230, 20, ILI9341_BLACK);
      tft.setTextSize(1);
      tft.setTextColor(currentTheme.primary);
      tft.setCursor(5, 80);
      tft.print(outputText);
    }
  }

  Serial.print("Button pressed: ");
  Serial.print(btn->label);
  Serial.print(", Alphabet index: ");
  Serial.print(alphabetIndex);
  Serial.print(", Selected char: ");
  Serial.println(selectedChar);
}

// FIXED: Removed needsFullRedraw optimization
void listBuiltInSignals() {
  buttonCount = 0;
  onBuiltInSignals = true;
  onBuiltInBrands = false;
  builtInSignalCount = 0;

  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
  drawHeaderFooter();

  int brandIndex = -1;
  for (int i = 0; i < hardcodedBrandsLength; i++) {
    if (String(hardcodedBrands[i].brandName) == currentBrandPath) {
      brandIndex = i;
      break;
    }
  }

  if (brandIndex == -1) {
    tft.setTextSize(2);
    tft.setTextColor(currentTheme.primary);
    tft.setCursor(20, 120);
    tft.println("Brand not");
    tft.setCursor(40, 140);
    tft.println("found!");

    drawBackBtn(60, 200, 120, 40, builtInSignalsBrowser);
    drawTitle("Brand signals", 75);
    return;
  }

  const IRCode *brandCodes = hardcodedBrands[brandIndex].codes;
  builtInSignalCount = hardcodedBrands[brandIndex].codesLength;

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
      tft.setTextColor(ILI9341_BLACK);
    } else {
      tft.setTextColor(ILI9341_WHITE);
    }

    tft.setTextSize(2);
    tft.setCursor(10, yPos + 5);
    tft.println(brandCodes[i].codeName);
    yPos += lineHeight;
  }

  if (builtInSignalScrollOffset > 0) {
    tft.fillTriangle(225, 85, 220, 90, 230, 90, ILI9341_WHITE);
  }
  if (builtInSignalScrollOffset + maxVisible < builtInSignalCount) {
    tft.fillTriangle(225, 225, 220, 220, 230, 220, ILI9341_WHITE);
  }

  createTouchBox(10, 235, 70, 30, currentTheme.secondary, currentTheme.secondary, "Up", []() {
    if (builtInSignalHighlightedIndex > 0) {
      builtInSignalHighlightedIndex--;
      if (builtInSignalHighlightedIndex < builtInSignalScrollOffset) {
        builtInSignalScrollOffset = builtInSignalHighlightedIndex;
      }
      listBuiltInSignals();
    }
  });

  // FIXED: Changed builtInBrandScrollOffset to builtInSignalScrollOffset
  createTouchBox(85, 235, 70, 30, currentTheme.secondary, currentTheme.secondary, "Down", []() {
    if (builtInSignalHighlightedIndex < builtInSignalCount - 1) {
      builtInSignalHighlightedIndex++;
      if (builtInSignalHighlightedIndex >= builtInSignalScrollOffset + 5) {
        builtInSignalScrollOffset = builtInSignalHighlightedIndex - 5 + 1;
      }
      listBuiltInSignals();
    }
  });

  createTouchBox(160, 235, 70, 30, currentTheme.primary, currentTheme.primary, "Send", []() {
    int brandIndex = -1;
    for (int i = 0; i < hardcodedBrandsLength; i++) {
      if (String(hardcodedBrands[i].brandName) == currentBrandPath) {
        brandIndex = i;
        break;
      }
    }

    if (brandIndex != -1) {
      const IRCode *selectedCode = &hardcodedBrands[brandIndex].codes[builtInSignalHighlightedIndex];

      IRSignal signal;
      strncpy(signal.name, selectedCode->codeName, 10);
      signal.rawDataLen = 0;

      for (int i = 0; i < 150; i++) {
        if (selectedCode->codeArray[i] == 0 && i > 10) {
          break;
        }
        signal.rawData[i] = selectedCode->codeArray[i];
        signal.rawDataLen++;
      }

      transmitSignal(signal);
    }
  });

  tft.fillRect(10, 270, 220, 25, ILI9341_BLACK);
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

  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
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
      tft.setTextColor(ILI9341_BLACK);
    } else {
      tft.setTextColor(ILI9341_WHITE);
    }

    tft.setTextSize(2);
    tft.setCursor(10, yPos + 5);
    tft.println(builtInBrands[i]);
    yPos += lineHeight;
  }

  if (builtInBrandScrollOffset > 0) {
    tft.fillTriangle(225, 85, 220, 90, 230, 90, ILI9341_WHITE);
  }
  if (builtInBrandScrollOffset + maxVisible < builtInBrandCount) {
    tft.fillTriangle(225, 225, 220, 220, 230, 220, ILI9341_WHITE);
  }

  createTouchBox(10, 235, 70, 30, currentTheme.secondary, currentTheme.secondary, "Up", []() {
    if (builtInBrandHighlightedIndex > 0) {
      builtInBrandHighlightedIndex--;
      if (builtInBrandHighlightedIndex < builtInBrandScrollOffset) {
        builtInBrandScrollOffset = builtInBrandHighlightedIndex;
      }
      builtInSignalsBrowser();
    }
  });

  createTouchBox(85, 235, 70, 30, currentTheme.secondary, currentTheme.secondary, "Down", []() {
    if (builtInBrandHighlightedIndex < builtInBrandCount - 1) {
      builtInBrandHighlightedIndex++;
      if (builtInBrandHighlightedIndex >= builtInBrandScrollOffset + 5) {
        builtInBrandScrollOffset = builtInBrandHighlightedIndex - 5 + 1;
      }
      builtInSignalsBrowser();
    }
  });

  createTouchBox(160, 235, 70, 30, currentTheme.primary, currentTheme.primary, "Open", []() {
    currentBrandPath = builtInBrands[builtInBrandHighlightedIndex];
    builtInSignalHighlightedIndex = 0;
    builtInSignalScrollOffset = 0;
    listBuiltInSignals();
  });

  tft.fillRect(10, 270, 220, 25, ILI9341_BLACK);
  createTouchBox(10, 270, 220, 25, currentTheme.secondary, currentTheme.secondary, "Back", drawMenuUI, true);

  drawTitle("Built-in signals", 70);
}

void sdData() {
  createOptions(SD_CARD_OPTIONS, 4, 10, 70);
  drawTitle("SD Card options", 75);
}

void showFileOptions() {
  buttonCount = 0;
  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
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
  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
  drawHeaderFooter();
  drawTitle("SD Card > Info", 75);

  uint64_t totalBytes = SD.totalBytes();
  uint64_t usedBytes = SD.usedBytes();
  uint64_t freeBytes = totalBytes - usedBytes;

  int imageCount = countFilesInDirectory("/images");
  int savedSignalsCount = countFilesInDirectory("/saved-signals");
  int builtInSignalsCount = countFilesInDirectory("/built-in-signals");

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
  tft.println("Files");
  tft.drawFastHLine(0, yPos + 26, 240, currentTheme.primary);
  tft.drawFastHLine(0, yPos + 28, 240, currentTheme.primary);
  yPos += 40;

  tft.setTextSize(2);
  tft.setCursor(10, yPos);
  tft.print("Images: ");
  tft.println(imageCount);
  yPos += 20;

  tft.setCursor(10, yPos);
  tft.print("Saved signals: ");
  tft.println(savedSignalsCount);
  yPos += 20;

  tft.setCursor(10, yPos);
  tft.print("Built-in signals:");
  tft.println(builtInSignalsCount);

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
  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
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
      tft.setTextColor(ILI9341_BLACK);
    } else {
      tft.fillRect(5, highlightY, 230, highlightHeight, ILI9341_BLACK);
      tft.setTextColor(ILI9341_WHITE);
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
    tft.fillTriangle(225, 85, 220, 90, 230, 90, ILI9341_WHITE);
  }
  if (sdScrollOffset + sdMaxVisible < sdFileCount) {
    tft.fillTriangle(225, 220, 220, 215, 230, 215, ILI9341_WHITE);
  }

  createTouchBox(10, 240, 70, 30, currentTheme.secondary, currentTheme.secondary, "Up", []() {
    if (sdHighlightedIndex > 0) {
      sdHighlightedIndex--;
      if (sdHighlightedIndex < sdScrollOffset) {
        sdScrollOffset = sdHighlightedIndex;
      }
      drawSDFileBrowser();
    }
  });

  createTouchBox(85, 240, 70, 30, currentTheme.secondary, currentTheme.secondary, "Down", []() {
    if (sdHighlightedIndex < sdFileCount - 1) {
      sdHighlightedIndex++;
      if (sdHighlightedIndex >= sdScrollOffset + sdMaxVisible) {
        sdScrollOffset = sdHighlightedIndex - sdMaxVisible + 1;
      }
      drawSDFileBrowser();
    }
  });

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
    createTouchBox(160, 240, 70, 30, 0xF800, ILI9341_WHITE, "Del", deleteSelectedFile);
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
  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
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
    tft.fillRect(0, 130, 240, 50, ILI9341_BLACK);
    String entryName = String(entry.name());
    bool isDir = entry.isDirectory();
    entry.close();
    tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);

    if (entryName == "System Volume Information" || entryName == "built-in-signals") {
      tft.setCursor(10, 130);
      tft.setTextColor(currentTheme.primary);
      tft.print("Skipping:");
      tft.setCursor(0, 160);
      tft.setTextColor(ILI9341_WHITE);
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
    tft.setTextColor(ILI9341_WHITE);
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
      tft.setTextColor(ILI9341_WHITE);
      tft.println(entryName);
      delay(1000);
    } else {
      tft.setCursor(10, 130);
      tft.setTextColor(0xF800);
      tft.println("Failed:");
      tft.setCursor(0, 160);
      tft.setTextColor(ILI9341_WHITE);
      tft.println(entryName);
      delay(1000);
    }

    entry = root.openNextFile();
  }

  root.close();

  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
  tft.setTextColor(0x07E0);
  tft.setTextSize(2);
  tft.setCursor(20, 140);
  tft.println("Formatting done!");

  tft.setTextColor(ILI9341_WHITE);
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
void listGroupedSignals() {
  buttonCount = 0;
  onSavedSignals = true;
  onSavedSignalGroups = false;
  groupedSignalCount = 0;

  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
  drawHeaderFooter();

  File dir = SD.open("/saved-signals");
  if (!dir) {
    tft.setTextSize(2);
    tft.setTextColor(currentTheme.primary);
    tft.setCursor(20, 120);
    tft.println("Error opening");
    tft.setCursor(40, 140);
    tft.println("directory!");

    drawBackBtn(60, 200, 120, 40, listSavedSignals);
    drawTitle("Group signals", 75);
    return;
  }

  File entry = dir.openNextFile();
  while (entry && groupedSignalCount < 50) {
    if (!entry.isDirectory()) {
      String filename = String(entry.name());
      String prefix = extractPrefix(filename);

      if (prefix == currentSavedGroup) {
        groupedSignalFiles[groupedSignalCount] = filename;
        groupedSignalCount++;
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();

  if (groupedSignalCount == 0) {
    tft.setTextSize(2);
    tft.setTextColor(currentTheme.primary);
    tft.setCursor(30, 120);
    tft.println("No signals");
    tft.setCursor(50, 140);
    tft.println("in group!");

    drawBackBtn(60, 200, 120, 40, listSavedSignals);
    drawTitle("Group signals", 75);
    return;
  }

  int maxVisible = 5;
  int yPos = 70;
  int lineHeight = 30;

  for (int i = groupedSignalScrollOffset; i < min(groupedSignalScrollOffset + maxVisible, groupedSignalCount); i++) {
    int highlightY = yPos;
    int highlightHeight = 25;

    if (i == groupedSignalHighlightedIndex) {
      tft.fillRect(5, highlightY, 230, highlightHeight, currentTheme.primary);
      tft.setTextColor(ILI9341_BLACK);
    } else {
      tft.setTextColor(ILI9341_WHITE);
    }

    tft.setTextSize(2);
    tft.setCursor(10, yPos + 5);
    String displayName = groupedSignalFiles[i];
    displayName.replace(".bin", "");
    int hyphenIndex = displayName.indexOf('-');
    if (hyphenIndex >= 0) {
      displayName = displayName.substring(hyphenIndex + 1);
    }
    tft.println(displayName);
    yPos += lineHeight;
  }

  if (groupedSignalScrollOffset > 0) {
    tft.fillTriangle(225, 85, 220, 90, 230, 90, ILI9341_WHITE);
  }
  if (groupedSignalScrollOffset + maxVisible < groupedSignalCount) {
    tft.fillTriangle(225, 225, 220, 220, 230, 220, ILI9341_WHITE);
  }

  createTouchBox(10, 235, 70, 30, currentTheme.secondary, currentTheme.secondary, "Up", []() {
    if (groupedSignalHighlightedIndex > 0) {
      groupedSignalHighlightedIndex--;
      if (groupedSignalHighlightedIndex < groupedSignalScrollOffset) {
        groupedSignalScrollOffset = groupedSignalHighlightedIndex;
      }
      listGroupedSignals();
    }
  });

  createTouchBox(85, 235, 70, 30, currentTheme.secondary, currentTheme.secondary, "Down", []() {
    if (groupedSignalHighlightedIndex < groupedSignalCount - 1) {
      groupedSignalHighlightedIndex++;
      if (groupedSignalHighlightedIndex >= groupedSignalScrollOffset + 5) {
        groupedSignalScrollOffset = groupedSignalHighlightedIndex - 5 + 1;
      }
      listGroupedSignals();
    }
  });

  // FIXED: Added SPI management before transmission
  createTouchBox(160, 235, 70, 30, currentTheme.primary, currentTheme.primary, "Send", []() {
    String filepath = "/saved-signals/" + groupedSignalFiles[groupedSignalHighlightedIndex];
    File file = SD.open(filepath.c_str(), FILE_READ);
    if (file) {
      IRSignal signal;
      file.read((uint8_t *)&signal, sizeof(IRSignal));
      file.close();

      // FIXED: Deselect SD card before IR transmission
      digitalWrite(SD_CS, HIGH);
      digitalWrite(TOUCH_CS, HIGH);
      delay(10);

      transmitSignal(signal);
    }
  });

  tft.fillRect(10, 270, 220, 25, ILI9341_BLACK);
  createTouchBox(
    10, 270, 220, 25, currentTheme.secondary, currentTheme.secondary, "Back", []() {
      onSavedSignals = false;
      listSavedSignals();
    },
    true);

  String title = currentSavedGroup + " signals";
  drawTitle(title.c_str(), 70);
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
      tft.fillRect(0, 60, 240, 100, ILI9341_BLACK);
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

void processTouchButtons(int tx, int ty) {
  unsigned long currentTime = millis();

  for (int i = 0; i < buttonCount; i++) {
    TouchButton *btn = &buttons[i];

    if (isTouchInButton(btn, tx, ty)) {
      if (!btn->pressed) {
        btn->pressed = true;
        drawButton(btn, true);

        activeBtnIndex = i;

        if (btn->callback != NULL && (currentTime - lastTouchTime > DEBOUNCE_DELAY)) {
          lastTouchTime = currentTime;
          btn->callback();
        }
      }
    } else {
      if (btn->pressed) {
        btn->pressed = false;
        drawButton(btn, false);
      }
    }
  }
}

void createTouchBox(int x, int y, int width, int height, uint16_t color, uint16_t textColor, const char *label, void (*callback)(), bool isBackButton) {
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

  drawButton(btn, false);

  buttonCount++;
}

bool isTouchInButton(TouchButton *btn, int tx, int ty) {
  return (tx >= btn->x && tx <= (btn->x + btn->w) && ty >= btn->y && ty <= (btn->y + btn->h));
}

void drawButton(TouchButton *btn, bool active) {
  if (btn->isBackButton) {
    active = !active;
  }

  tft.fillRect(btn->x, btn->y, btn->w, btn->h, ILI9341_BLACK);

  if (active) {
    tft.fillRect(btn->x, btn->y, btn->w, btn->h, btn->color);
    tft.drawRect(btn->x, btn->y, btn->w, btn->h, ILI9341_WHITE);

    int cornerSize = 8;
    tft.drawFastHLine(btn->x, btn->y, cornerSize, ILI9341_WHITE);
    tft.drawFastVLine(btn->x, btn->y, cornerSize, ILI9341_WHITE);
    tft.drawFastHLine(btn->x + btn->w - cornerSize, btn->y, cornerSize, ILI9341_WHITE);
    tft.drawFastVLine(btn->x + btn->w - 1, btn->y, cornerSize, ILI9341_WHITE);
    tft.drawFastHLine(btn->x, btn->y + btn->h - 1, cornerSize, ILI9341_WHITE);
    tft.drawFastVLine(btn->x, btn->y + btn->h - cornerSize, cornerSize, ILI9341_WHITE);
    tft.drawFastHLine(btn->x + btn->w - cornerSize, btn->y + btn->h - 1, cornerSize, ILI9341_WHITE);
    tft.drawFastVLine(btn->x + btn->w - 1, btn->y + btn->h - cornerSize, cornerSize, ILI9341_WHITE);

    tft.setTextColor(ILI9341_BLACK);
    tft.setTextSize(2);
  } else {
    tft.drawRect(btn->x - 1, btn->y - 1, btn->w + 2, btn->h + 2, currentTheme.accent);
    tft.drawRect(btn->x - 2, btn->y - 2, btn->w + 4, btn->h + 4, currentTheme.darkest);
    tft.fillRect(btn->x, btn->y, btn->w, btn->h, ILI9341_BLACK);
    tft.drawRect(btn->x, btn->y, btn->w, btn->h, btn->color);

    int cornerSize = 8;
    tft.drawFastHLine(btn->x, btn->y, cornerSize, btn->color);
    tft.drawFastVLine(btn->x, btn->y, cornerSize, btn->color);
    tft.drawFastHLine(btn->x + btn->w - cornerSize, btn->y, cornerSize, btn->color);
    tft.drawFastVLine(btn->x + btn->w - 1, btn->y, cornerSize, btn->color);
    tft.drawFastHLine(btn->x, btn->y + btn->h - 1, cornerSize, btn->color);
    tft.drawFastVLine(btn->x, btn->y + btn->h - cornerSize, cornerSize, btn->color);
    tft.drawFastHLine(btn->x + btn->w - cornerSize, btn->y + btn->h - 1, cornerSize, btn->color);
    tft.drawFastVLine(btn->x + btn->w - 1, btn->y + btn->h - cornerSize, cornerSize, btn->color);

    for (int j = 2; j < btn->h - 2; j += 5) {
      tft.drawFastHLine(btn->x + 2, btn->y + j, btn->w - 4, currentTheme.darkest);
    }

    tft.setTextColor(btn->color);
    tft.setTextSize(2);
  }

  int16_t x1, y1;
  uint16_t w, h;

  tft.getTextBounds(btn->label, 0, 0, &x1, &y1, &w, &h);

  int16_t textX = btn->x + (btn->w - w) / 2;
  int16_t textY = btn->y + (btn->h - h) / 2;

  if (active) {
    tft.setTextColor(ILI9341_BLACK, btn->color);
  } else {
    tft.setTextColor(btn->color, ILI9341_BLACK);
  }

  tft.setCursor(textX, textY);
  tft.print(btn->label);
}

void createOptions(const Option options[], int count, int x, int y, int btnWidth, int btnHeight) {
  buttonCount = 0;
  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);

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
// =============================== IMAGE DISPLAY =====================================
// ===================================================================================

uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read();
  ((uint8_t *)&result)[1] = f.read();
  return result;
}

uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read();
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read();
  return result;
}

void drawBMP(const char *filename, int16_t x, int16_t y) {
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);
  delay(10);

  File bmpFile = SD.open(filename);
  if (!bmpFile) {
    return;
  }

  if (read16(bmpFile) != 0x4D42) {
    bmpFile.close();
    return;
  }

  read32(bmpFile);
  read32(bmpFile);
  uint32_t bmpImageOffset = read32(bmpFile);

  read32(bmpFile);
  int32_t bmpWidth = read32(bmpFile);
  int32_t bmpHeight = read32(bmpFile);

  read16(bmpFile);
  uint16_t bmpDepth = read16(bmpFile);
  read32(bmpFile);

  if (bmpDepth != 24) {
    bmpFile.close();
    return;
  }

  if (bmpHeight < 0)
    bmpHeight = -bmpHeight;
  uint32_t rowSize = (bmpWidth * 3 + 3) & ~3;

  uint8_t sdbuffer[3 * 20];

  digitalWrite(SD_CS, HIGH);
  delay(10);

  for (int row = 0; row < bmpHeight; row++) {
    digitalWrite(TFT_CS, HIGH);
    delay(1);

    bmpFile.seek(bmpImageOffset + (bmpHeight - 1 - row) * rowSize);

    int col = 0;
    while (col < bmpWidth) {
      int chunk = min(20, (int)(bmpWidth - col));
      bmpFile.read(sdbuffer, 3 * chunk);

      digitalWrite(SD_CS, HIGH);
      delay(1);

      for (int i = 0; i < chunk; i++) {
        uint8_t b = sdbuffer[i * 3 + 0];
        uint8_t g = sdbuffer[i * 3 + 1];
        uint8_t r = sdbuffer[i * 3 + 2];
        uint16_t color = tft.color565(r, g, b);
        tft.drawPixel(x + col + i, y + row, color);
      }
      col += chunk;
    }
  }

  digitalWrite(TFT_CS, HIGH);
  bmpFile.close();
}

// ===================================================================================
// ================================= IR Rx & Tx ======================================
// ===================================================================================

void sendFixSignal(const uint16_t *signalData, size_t length) {
  IrSender.sendRaw(signalData, length, 38);
}