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
  String allPrefixes[50];
  int prefixCount = 0;

  File entry = dir.openNextFile();
  while (entry && prefixCount < 50) {
    if (!entry.isDirectory()) {
      String filename = String(entry.name());
      String prefix = extractPrefix(filename);

      // Check if prefix already exists
      bool exists = false;
      for (int i = 0; i < savedSignalGroupCount; i++) {
        if (savedSignalGroups[i] == prefix) {
          exists = true;
          break;
        }
      }

      if (!exists && savedSignalGroupCount < 20) {
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
      tft.fillRect(5, highlightY, 230, highlightHeight, ILI9341_BLACK);
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

  // Back button
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
  // Ha nincs új jel, kilépünk
  if (!IrReceiver.decode()) return;

  // Ellenőrizzük, van-e nyers adat
  if (IrReceiver.decodedIRData.rawlen == 0) {
    IrReceiver.resume();
    return;
  }

  // A nyers adat hosszát beállítjuk
  currentRawDataLen = IrReceiver.decodedIRData.rawlen - 1;

  // Átmásoljuk a nyers impulzusokat az irparams.rawbuf-ból
  for (uint16_t i = 1; i < IrReceiver.decodedIRData.rawlen; i++) {
    uint32_t duration = IrReceiver.irparams.rawbuf[i] * MICROS_PER_TICK;
    currentRawData[i - 1] = duration;
  }

  signalCaptured = true;

  // Felkészítjük a következő jelre
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

  // Draw output text at the top with 5px gap
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(5, 70);  // Moved up to make space for keyboard
  tft.print("Save as: ");
  tft.setTextColor(currentTheme.primary);
  tft.setCursor(5, 80);  // 5px gap below "Save as:"
  tft.print(outputText);

  // Key sizes - 7 keys per row max
  const int cellWidth = 32;
  const int cellHeight = 32;
  const int spaceWidth = cellWidth * 3;
  const int startX = 8;
  const int startY = 105;  // Moved down to position keyboard closer to bottom

  // Create keyboard buttons
  for (int i = 0; i < 29; i++) {
    const char *currentChar = alphabet[i];  // Use const char* directly
    int currentCellWidth = cellWidth;
    uint16_t btnColor = currentTheme.primary;
    uint16_t btnTextColor = ILI9341_WHITE;
    int row, col;

    // Row 0: Q W E R T Z U (indices 0-6)
    if (i >= 0 && i <= 6) {
      row = 0;
      col = i;
    }
    // Row 1: I O P A S D F (indices 7-9, 10-13)
    else if (i == 7) {
      row = 1;
      col = 0;
    }  // I
    else if (i == 8) {
      row = 1;
      col = 1;
    }  // O
    else if (i == 9) {
      row = 1;
      col = 2;
    }  // P
    else if (i >= 10 && i <= 13) {
      row = 1;
      col = i - 7;  // 10->3, 11->4, 12->5, 13->6
    }
    // Row 2: G H J K L Y X (indices 14-18, 19-20)
    else if (i >= 14 && i <= 18) {
      row = 2;
      col = i - 14;  // 14->0, 15->1, 16->2, 17->3, 18->4
    } else if (i >= 19 && i <= 20) {
      row = 2;
      col = i - 14;  // 19->5, 20->6
    }
    // Row 3: [empty] C V B N M [empty] (indices 21-22, 24-26)
    else if (i == 21) {
      row = 3;
      col = 1;
    }  // C
    else if (i == 22) {
      row = 3;
      col = 2;
    }  // V
    else if (i == 24) {
      row = 3;
      col = 3;
    }  // B
    else if (i == 25) {
      row = 3;
      col = 4;
    }  // N
    else if (i == 26) {
      row = 3;
      col = 5;
    }  // M
    // Row 4: < [SPACE taking 3 places] > (indices 27, 23, 28)
    else if (i == 27) {  // < (backspace - RED with WHITE text)
      row = 4;
      col = 1;
      btnColor = 0xF800;  // Red
      btnTextColor = ILI9341_WHITE;
    } else if (i == 23) {  // - (hyphen - 3 key width)
      row = 4;
      col = 2;
      currentCellWidth = spaceWidth;
    } else if (i == 28) {  // > (save - GREEN)
      row = 4;
      col = 5;
      btnColor = 0x07E0;  // Green
      btnTextColor = ILI9341_WHITE;
    } else continue;

    int xOffset = startX + col * cellWidth;
    int yOffset = startY + row * cellHeight;

    // Create touch button - use the global alphabet array directly
    createTouchBox(xOffset, yOffset, currentCellWidth, cellHeight,
                   btnColor, btnTextColor,
                   currentChar,  // Use the const char* directly
                   keyboardButtonPressed);
  }

  // Small back button at the very bottom, under the keyboard
  // Position it below the last keyboard row with proper spacing
  int lastKeyboardRowY = startY + (4 * cellHeight);   // Row 4 is the last keyboard row
  int backBtnY = lastKeyboardRowY + cellHeight + 10;  // 10px below keyboard
  int backBtnWidth = 80;                              // Smaller width
  int backBtnHeight = 25;                             // Smaller height
  int backBtnX = (240 - backBtnWidth) / 2;            // Centered

  // Clear the area where the back button will be to remove any overlapping content
  tft.fillRect(backBtnX, backBtnY, backBtnWidth, backBtnHeight, ILI9341_BLACK);

  createTouchBox(
    backBtnX, backBtnY, backBtnWidth, backBtnHeight,
    currentTheme.secondary, currentTheme.secondary,
    "Back",
    []() {
      outputText[0] = '\0';
      signalCaptured = false;
      onKeyboard = false;
      buttonCount = 0;  // Clear all buttons before switching
      signalOptions();
    },
    true);  // Mark as back button

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

  // Get the pressed button
  TouchButton *btn = &buttons[activeBtnIndex];

  // Find which alphabet character corresponds to this button's label
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
    // Backspace
    int len = strlen(outputText);
    if (len > 0) outputText[len - 1] = '\0';

    // Update display with new positioning
    tft.fillRect(5, 80, 230, 20, ILI9341_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(currentTheme.primary);
    tft.setCursor(5, 80);
    tft.print(outputText);

  } else if (selectedChar == "_") {
    // Space - REPLACE WITH HYPHEN
    int len = strlen(outputText);
    if (len < MAX_SAVED_SIGNAL_CHARS) {
      outputText[len] = '-';  // Use hyphen instead of space
      outputText[len + 1] = '\0';

      // Update display with new positioning
      tft.fillRect(5, 80, 230, 20, ILI9341_BLACK);
      tft.setTextSize(1);
      tft.setTextColor(currentTheme.primary);
      tft.setCursor(5, 80);
      tft.print(outputText);
    }

  } else if (selectedChar == ">") {
    // Save
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
    // Regular character
    int len = strlen(outputText);
    if (len < MAX_SAVED_SIGNAL_CHARS) {
      outputText[len] = selectedChar[0];
      outputText[len + 1] = '\0';

      // Update display with new positioning
      tft.fillRect(5, 80, 230, 20, ILI9341_BLACK);
      tft.setTextSize(1);
      tft.setTextColor(currentTheme.primary);
      tft.setCursor(5, 80);
      tft.print(outputText);
    }
  }

  // Debug output to Serial
  Serial.print("Button pressed: ");
  Serial.print(btn->label);
  Serial.print(", Alphabet index: ");
  Serial.print(alphabetIndex);
  Serial.print(", Selected char: ");
  Serial.println(selectedChar);
}

void listBuiltInSignals() {
  buttonCount = 0;
  onBuiltInSignals = true;
  onBuiltInBrands = false;
  builtInSignalCount = 0;
  // Don't reset navigation state here - let it persist for up/down navigation

  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
  drawHeaderFooter();

  // Find the selected brand in hardcoded array
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

  // Load signals from hardcoded brand
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

  // Display signals - 5 visible at a time
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
      tft.fillRect(5, highlightY, 230, highlightHeight, ILI9341_BLACK);
      tft.setTextColor(ILI9341_WHITE);
    }

    tft.setTextSize(2);
    tft.setCursor(10, yPos + 5);
    tft.println(brandCodes[i].codeName);
    yPos += lineHeight;
  }

  // Scroll indicators - RIGHT SIDE, WHITE
  if (builtInSignalScrollOffset > 0) {
    tft.fillTriangle(225, 85, 220, 90, 230, 90, ILI9341_WHITE);
  }
  if (builtInSignalScrollOffset + maxVisible < builtInSignalCount) {
    tft.fillTriangle(225, 225, 220, 220, 230, 220, ILI9341_WHITE);
  }

  // Navigation buttons
  createTouchBox(10, 235, 70, 30, currentTheme.secondary, currentTheme.secondary, "Up", []() {
    if (builtInSignalHighlightedIndex > 0) {
      builtInSignalHighlightedIndex--;
      if (builtInSignalHighlightedIndex < builtInSignalScrollOffset) {
        builtInSignalScrollOffset = builtInSignalHighlightedIndex;
      }
      listBuiltInSignals();
    }
  });

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
    // Find the selected brand and signal
    int brandIndex = -1;
    for (int i = 0; i < hardcodedBrandsLength; i++) {
      if (String(hardcodedBrands[i].brandName) == currentBrandPath) {
        brandIndex = i;
        break;
      }
    }

    if (brandIndex != -1) {
      const IRCode *selectedCode = &hardcodedBrands[brandIndex].codes[builtInSignalHighlightedIndex];

      // Convert IRCode to IRSignal and transmit
      IRSignal signal;
      strncpy(signal.name, selectedCode->codeName, 10);
      signal.rawDataLen = 0;

      // Calculate the actual length of the signal data (until we hit 0)
      for (int i = 0; i < 150; i++) {
        if (selectedCode->codeArray[i] == 0 && i > 10) {  // Allow some zeros but not at start
          break;
        }
        signal.rawData[i] = selectedCode->codeArray[i];
        signal.rawDataLen++;
      }

      transmitSignal(signal);
    }
  });

  // Back button
  tft.fillRect(10, 270, 220, 25, ILI9341_BLACK);
  createTouchBox(
    10, 270, 220, 25, currentTheme.secondary, currentTheme.secondary, "Back", []() {
      onBuiltInSignals = false;
      builtInSignalsBrowser();
    },
    true);

  // Display brand name for title
  String title = currentBrandPath + " signals";
  drawTitle(title.c_str(), 70);
}

void builtInSignalsBrowser() {
  buttonCount = 0;
  onBuiltInBrands = true;
  builtInBrandCount = 0;
  // Don't reset navigation state here - let it persist for up/down navigation

  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
  drawHeaderFooter();

  // Load brands from hardcoded array (always available)
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

  // Display brands - 5 visible at a time
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
      tft.fillRect(5, highlightY, 230, highlightHeight, ILI9341_BLACK);
      tft.setTextColor(ILI9341_WHITE);
    }

    tft.setTextSize(2);
    tft.setCursor(10, yPos + 5);
    tft.println(builtInBrands[i]);
    yPos += lineHeight;
  }

  // Scroll indicators - RIGHT SIDE, WHITE
  if (builtInBrandScrollOffset > 0) {
    tft.fillTriangle(225, 85, 220, 90, 230, 90, ILI9341_WHITE);
  }
  if (builtInBrandScrollOffset + maxVisible < builtInBrandCount) {
    tft.fillTriangle(225, 225, 220, 220, 230, 220, ILI9341_WHITE);
  }

  // Navigation buttons
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
    // Store the selected brand name (no path needed for hardcoded signals)
    currentBrandPath = builtInBrands[builtInBrandHighlightedIndex];
    // Reset signal navigation when opening a new brand
    builtInSignalHighlightedIndex = 0;
    builtInSignalScrollOffset = 0;
    listBuiltInSignals();
  });

  // Back button
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

  // Remove size info from display name to get actual filename
  String actualFileName = selectedFile;
  if (!selectedFile.startsWith("[DIR] ")) {
    int parenIndex = actualFileName.lastIndexOf('(');
    if (parenIndex > 0) {
      actualFileName = actualFileName.substring(0, parenIndex - 1);
    }
  } else {
    actualFileName = actualFileName.substring(6);  // Remove "[DIR] "
  }

  // Display selected file info
  tft.setTextSize(2);
  tft.setTextColor(currentTheme.primary);
  tft.setCursor(10, 80);
  tft.print("Selected:");
  tft.setCursor(10, 105);

  // Truncate long names
  if (actualFileName.length() > 20) {
    actualFileName = actualFileName.substring(0, 17) + "...";
  }
  tft.println(actualFileName);

  // File operation buttons
  if (!selectedFile.startsWith("[DIR] ")) {
    // File operations
    createTouchBox(30, 140, 180, 30, currentTheme.primary, currentTheme.primary, "Delete File", []() {
      // Add file deletion logic here
      Serial.println("Delete file functionality to be implemented");
    });

    createTouchBox(30, 180, 180, 30, currentTheme.primary, currentTheme.primary, "Rename File", []() {
      // Add file rename logic here
      Serial.println("Rename file functionality to be implemented");
    });
  }

  // Back to file browser
  createTouchBox(60, 240, 120, 30, currentTheme.secondary, currentTheme.secondary, "Back", drawSDFileBrowser, true);

  drawTitle("File Options", 85);
}

void listSDInfo() {
  buttonCount = 0;
  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
  drawHeaderFooter();
  drawTitle("SD Card > Info", 75);

  // Get storage information
  uint64_t totalBytes = SD.totalBytes();
  uint64_t usedBytes = SD.usedBytes();
  uint64_t freeBytes = totalBytes - usedBytes;

  // Count files in directories
  int imageCount = countFilesInDirectory("/images");
  int savedSignalsCount = countFilesInDirectory("/saved-signals");
  int builtInSignalsCount = countFilesInDirectory("/built-in-signals");

  int yPos = 110;

  tft.setTextSize(3);
  tft.setTextColor(currentTheme.primary);

  // Storage header
  tft.setCursor(5, 72);
  tft.println("Storage");
  tft.drawFastHLine(0, yPos - 12, 240, currentTheme.primary);
  tft.drawFastHLine(0, yPos - 10, 240, currentTheme.primary);

  // Available storage in MB/GB
  tft.setTextSize(2);
  tft.setCursor(10, yPos);
  tft.print("Full: ");
  tft.println(formatBytes(totalBytes));
  yPos += 20;

  // Free storage in MB/GB
  tft.setCursor(10, yPos);
  tft.print("Free: ");
  tft.println(formatBytes(freeBytes));
  yPos += 20;

  // Used storage in MB/GB
  tft.setCursor(10, yPos);
  tft.print("Used: ");
  tft.println(formatBytes(usedBytes));
  yPos += 40;

  // Files header
  tft.setTextSize(3);
  tft.setCursor(5, yPos);
  tft.println("Files");
  tft.drawFastHLine(0, yPos + 26, 240, currentTheme.primary);
  tft.drawFastHLine(0, yPos + 28, 240, currentTheme.primary);
  yPos += 40;

  // Images count
  tft.setTextSize(2);
  tft.setCursor(10, yPos);
  tft.print("Images: ");
  tft.println(imageCount);
  yPos += 20;

  // Saved signals count
  tft.setCursor(10, yPos);
  tft.print("Saved signals: ");
  tft.println(savedSignalsCount);
  yPos += 20;

  // Built-in signals count
  tft.setCursor(10, yPos);
  tft.print("Built-in signals:");
  tft.println(builtInSignalsCount);

  // Back button
  drawBackBtn(185, 130, 55, 50, sdData);
}

String formatBytes(uint64_t bytes) {
  if (bytes < 1024) {
    // Less than 1 KB - show as bytes
    return String(bytes) + " B";
  } else if (bytes < 1024 * 1024) {
    // Less than 1 MB - show as KB
    return String(bytes / 1024.0, 1) + " KB";
  } else if (bytes < 1024 * 1024 * 1024) {
    // Less than 1 GB - show as MB
    return String(bytes / (1024.0 * 1024.0), 1) + " MB";
  } else {
    // 1 GB or more - show as GB
    return String(bytes / (1024.0 * 1024.0 * 1024.0), 1) + " GB";
  }
}

// Helper function to count files in a directory
int countFilesInDirectory(const char *directoryPath) {
  File dir = SD.open(directoryPath);
  int count = 0;

  if (!dir) {
    return 0;  // Directory doesn't exist
  }

  if (!dir.isDirectory()) {
    dir.close();
    return 0;  // Not a directory
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

    // Remove leading slash if present
    if (fileName.startsWith("/")) {
      fileName = fileName.substring(1);
    }

    if (entry.isDirectory()) {
      // Format: DIR - Dirname
      sdFiles[sdFileCount] = "DIR - " + fileName;
    } else {
      // Format: filename.extension - size
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

  // Display current path with smaller font
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

  // Display files - use global sdMaxVisible
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

  // Scroll indicators
  if (sdScrollOffset > 0) {
    tft.fillTriangle(225, 85, 220, 90, 230, 90, ILI9341_WHITE);
  }
  if (sdScrollOffset + sdMaxVisible < sdFileCount) {
    tft.fillTriangle(225, 240, 220, 235, 230, 235, ILI9341_WHITE);
  }

  // Navigation buttons
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

  // Check if selected item is a directory or file
  String selectedFile = sdFiles[sdHighlightedIndex];
  bool isDirectory = selectedFile.startsWith("DIR - ");

  // Changed: Show "Del" button for files, "Open" for directories
  if (isDirectory) {
    createTouchBox(160, 240, 70, 30, currentTheme.primary, currentTheme.primary, "Open", []() {
      String selectedFile = sdFiles[sdHighlightedIndex];
      String dirName = selectedFile.substring(6);  // Remove "DIR - " prefix

      // Update current path
      if (currentPath == "/") {
        currentPath = "/" + dirName;
      } else {
        currentPath = currentPath + "/" + dirName;
      }

      // Reload files for new directory
      loadSDFiles(currentPath);
      sdHighlightedIndex = 0;
      sdScrollOffset = 0;
      drawSDFileBrowser();
    });
  } else {
    // Show red "Del" button for files
    createTouchBox(160, 240, 70, 30, 0xF800, ILI9341_WHITE, "Del", deleteSelectedFile);
  }

  // Back button
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

  // Delete all files and subdirectories
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

    // Skip protected and built-in folders
    if (entryName == "System Volume Information" || entryName == "built-in-signals") {
      tft.setCursor(10, 130);
      tft.setTextColor(currentTheme.primary);
      tft.print("Skipping:");
      tft.setCursor(0, 160);
      tft.setTextColor(ILI9341_WHITE);
      tft.println(entryName);
      entry = root.openNextFile();
      delay(1000);  // Reduced delay for better UX
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
      tft.setTextColor(0x07E0);  // Green for success
      tft.println("Deleted:");
      tft.setCursor(0, 160);
      tft.setTextColor(ILI9341_WHITE);
      tft.println(entryName);
      delay(1000);  // Reduced delay
    } else {
      tft.setCursor(10, 130);
      tft.setTextColor(0xF800);  // Red for failure
      tft.println("Failed:");
      tft.setCursor(0, 160);
      tft.setTextColor(ILI9341_WHITE);
      tft.println(entryName);
      delay(1000);  // Reduced delay
    }

    entry = root.openNextFile();
  }

  root.close();

  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
  tft.setTextColor(0x07E0);  // Green for success
  tft.setTextSize(2);
  tft.setCursor(20, 140);
  tft.println("Formatting done!");

  // Show what was preserved
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.setCursor(30, 170);
  tft.println("Preserved: built-in-signals");

  delay(2000);

  // Recreate the saved-signals directory since we deleted it
  if (!SD.exists("/saved-signals")) {
    SD.mkdir("/saved-signals");
  }

  drawMenuUI();
}

String extractPrefix(String filename) {
  // Remove .bin extension first
  filename.replace(".bin", "");

  // Find first hyphen
  int hyphenIndex = filename.indexOf('-');

  if (hyphenIndex > 0) {
    return filename.substring(0, hyphenIndex);
  }

  // If no hyphen, return the whole filename as prefix
  return filename;
}

void listGroupedSignals() {
  buttonCount = 0;
  onSavedSignals = true;
  onSavedSignalGroups = false;
  groupedSignalCount = 0;

  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
  drawHeaderFooter();

  // Load signals matching the current group prefix
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

  // Display signals
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
      tft.fillRect(5, highlightY, 230, highlightHeight, ILI9341_BLACK);
      tft.setTextColor(ILI9341_WHITE);
    }

    tft.setTextSize(2);
    tft.setCursor(10, yPos + 5);
    // Remove .bin extension and prefix for cleaner display
    String displayName = groupedSignalFiles[i];
    displayName.replace(".bin", "");
    // Remove prefix and hyphen
    int hyphenIndex = displayName.indexOf('-');
    if (hyphenIndex >= 0) {
      displayName = displayName.substring(hyphenIndex + 1);
    }
    tft.println(displayName);
    yPos += lineHeight;
  }

  // Scroll indicators
  if (groupedSignalScrollOffset > 0) {
    tft.fillTriangle(225, 85, 220, 90, 230, 90, ILI9341_WHITE);
  }
  if (groupedSignalScrollOffset + maxVisible < groupedSignalCount) {
    tft.fillTriangle(225, 225, 220, 220, 230, 220, ILI9341_WHITE);
  }

  // Navigation buttons
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

  createTouchBox(160, 235, 70, 30, currentTheme.primary, currentTheme.primary, "Send", []() {
    // Load and transmit the selected signal
    String filepath = "/saved-signals/" + groupedSignalFiles[groupedSignalHighlightedIndex];
    File file = SD.open(filepath.c_str(), FILE_READ);
    if (file) {
      IRSignal signal;
      file.read((uint8_t *)&signal, sizeof(IRSignal));
      file.close();

      transmitSignal(signal);
    }
  });

  // Back button
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

  // Only delete if it's a file (not a directory)
  if (!selectedFile.startsWith("DIR - ")) {
    // Extract actual filename (remove size info)
    String actualFileName = selectedFile;
    int dashIndex = actualFileName.lastIndexOf(" - ");
    if (dashIndex > 0) {
      actualFileName = actualFileName.substring(0, dashIndex);
    }

    // Build full path
    String fullPath;
    if (currentPath == "/") {
      fullPath = "/" + actualFileName;
    } else {
      fullPath = currentPath + "/" + actualFileName;
    }

    // Delete the file
    bool success = SD.remove(fullPath.c_str());

    if (success) {
      // Reload the file list
      loadSDFiles(currentPath);

      // Adjust highlighted index if needed
      if (sdHighlightedIndex >= sdFileCount && sdFileCount > 0) {
        sdHighlightedIndex = sdFileCount - 1;
      }
      if (sdScrollOffset > sdHighlightedIndex) {
        sdScrollOffset = max(0, sdHighlightedIndex);
      }

      drawSDFileBrowser();
    } else {
      // Show error message
      tft.fillRect(0, 60, 240, 100, ILI9341_BLACK);
      tft.setTextSize(2);
      tft.setTextColor(0xF800);  // Red
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

// Main setTheme function
void setTheme(uint8_t themeIndex) {
  // Update current theme colors
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

  // Redraw the entire UI with new theme
  drawMenuUI();
}

// ===================================================================================
// ============================== TOUCH BUTTONS ======================================
// ===================================================================================

// Process touch for all buttons with futuristic feedback
void processTouchButtons(int tx, int ty) {
  unsigned long currentTime = millis();

  for (int i = 0; i < buttonCount; i++) {
    TouchButton *btn = &buttons[i];

    if (isTouchInButton(btn, tx, ty)) {
      if (!btn->pressed) {
        btn->pressed = true;
        drawButton(btn, true);

        // Set the active button index
        activeBtnIndex = i;

        // Call the callback function ONLY if enough time has passed
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

// Futuristic button with scanlines and glowing effect
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

  // Draw button in normal state
  drawButton(btn, false);

  buttonCount++;
}

// Check if a point is inside a button
bool isTouchInButton(TouchButton *btn, int tx, int ty) {
  return (tx >= btn->x && tx <= (btn->x + btn->w) && ty >= btn->y && ty <= (btn->y + btn->h));
}

// Draw a button in normal or active state
void drawButton(TouchButton *btn, bool active) {
  if (btn->isBackButton) {
    // Back button reversed logic
    active = !active;
  }

  // Clear the button area first
  tft.fillRect(btn->x, btn->y, btn->w, btn->h, ILI9341_BLACK);

  if (active) {
    // Active state - filled with button's color
    tft.fillRect(btn->x, btn->y, btn->w, btn->h, btn->color);
    tft.drawRect(btn->x, btn->y, btn->w, btn->h, ILI9341_WHITE);

    // Corner accents in white
    int cornerSize = 8;
    tft.drawFastHLine(btn->x, btn->y, cornerSize, ILI9341_WHITE);
    tft.drawFastVLine(btn->x, btn->y, cornerSize, ILI9341_WHITE);
    tft.drawFastHLine(btn->x + btn->w - cornerSize, btn->y, cornerSize, ILI9341_WHITE);
    tft.drawFastVLine(btn->x + btn->w - 1, btn->y, cornerSize, ILI9341_WHITE);
    tft.drawFastHLine(btn->x, btn->y + btn->h - 1, cornerSize, ILI9341_WHITE);
    tft.drawFastVLine(btn->x, btn->y + btn->h - cornerSize, cornerSize, ILI9341_WHITE);
    tft.drawFastHLine(btn->x + btn->w - cornerSize, btn->y + btn->h - 1, cornerSize, ILI9341_WHITE);
    tft.drawFastVLine(btn->x + btn->w - 1, btn->y + btn->h - cornerSize, cornerSize, ILI9341_WHITE);

    // Text in black
    tft.setTextColor(ILI9341_BLACK);
    tft.setTextSize(2);
  } else {
    // Normal state - outlined
    tft.drawRect(btn->x - 1, btn->y - 1, btn->w + 2, btn->h + 2, currentTheme.accent);
    tft.drawRect(btn->x - 2, btn->y - 2, btn->w + 4, btn->h + 4, currentTheme.darkest);
    tft.fillRect(btn->x, btn->y, btn->w, btn->h, ILI9341_BLACK);
    tft.drawRect(btn->x, btn->y, btn->w, btn->h, btn->color);

    // Corner decorations
    int cornerSize = 8;
    tft.drawFastHLine(btn->x, btn->y, cornerSize, btn->color);
    tft.drawFastVLine(btn->x, btn->y, cornerSize, btn->color);
    tft.drawFastHLine(btn->x + btn->w - cornerSize, btn->y, cornerSize, btn->color);
    tft.drawFastVLine(btn->x + btn->w - 1, btn->y, cornerSize, btn->color);
    tft.drawFastHLine(btn->x, btn->y + btn->h - 1, cornerSize, btn->color);
    tft.drawFastVLine(btn->x, btn->y + btn->h - cornerSize, cornerSize, btn->color);
    tft.drawFastHLine(btn->x + btn->w - cornerSize, btn->y + btn->h - 1, cornerSize, btn->color);
    tft.drawFastVLine(btn->x + btn->w - 1, btn->y + btn->h - cornerSize, cornerSize, btn->color);

    // Add scanline effect
    for (int j = 2; j < btn->h - 2; j += 5) {
      tft.drawFastHLine(btn->x + 2, btn->y + j, btn->w - 4, currentTheme.darkest);
    }

    // Text in button's color
    tft.setTextColor(btn->color);
    tft.setTextSize(2);
  }

  // Draw centered text
  int16_t x1, y1;
  uint16_t w, h;

  // Get text bounds with current text size
  tft.getTextBounds(btn->label, 0, 0, &x1, &y1, &w, &h);

  // Calculate center position
  int16_t textX = btn->x + (btn->w - w) / 2;
  int16_t textY = btn->y + (btn->h - h) / 2;

  // Set background color for text (to prevent corruption)
  if (active) {
    tft.setTextColor(ILI9341_BLACK, btn->color);  // Black text on button color background
  } else {
    tft.setTextColor(btn->color, ILI9341_BLACK);  // Button color text on black background
  }

  tft.setCursor(textX, textY);
  tft.print(btn->label);
}

// Create options using an array (vertical list)
void createOptions(const Option options[], int count, int x, int y, int btnWidth, int btnHeight) {
  buttonCount = 0;
  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);

  for (int i = 0; i < count; i++) {
    // Check if this is the last option and it's named "Back"
    bool isBack = (i == count - 1 && strcmp(options[i].name, "Back") == 0);
    createTouchBox(x, y + 15 + (btnHeight + 5) * i, btnWidth, btnHeight,
                   currentTheme.primary, currentTheme.primary,
                   options[i].name, options[i].callback, isBack);
  }

  // Redraw footer
  drawHeaderFooter();
}

// Create options in a grid layout (rows x cols)
void createGridOptions(const Option options[], int count, int rows, int cols, int startX, int startY, int spacing, int btnSize) {
  // Create buttons in grid
  int buttonIndex = 0;
  for (int row = 0; row < rows && buttonIndex < count; row++) {
    for (int col = 0; col < cols && buttonIndex < count; col++) {
      int x = startX + col * (btnSize + spacing);
      int y = startY + row * (btnSize + spacing);

      // Check if this is the last option and it's named "Back"
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

// Helper functions to read little-endian data
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
  // Ensure TFT is deselected before accessing SD
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);
  delay(10);

  File bmpFile = SD.open(filename);
  if (!bmpFile) {
    return;
  }

  // Verify BMP signature
  if (read16(bmpFile) != 0x4D42) {
    bmpFile.close();
    return;
  }

  read32(bmpFile);  // skip file size
  read32(bmpFile);  // skip creator bytes
  uint32_t bmpImageOffset = read32(bmpFile);

  read32(bmpFile);  // DIB header size
  int32_t bmpWidth = read32(bmpFile);
  int32_t bmpHeight = read32(bmpFile);

  read16(bmpFile);  // color planes
  uint16_t bmpDepth = read16(bmpFile);
  read32(bmpFile);  // compression

  if (bmpDepth != 24) {
    bmpFile.close();
    return;
  }

  if (bmpHeight < 0)
    bmpHeight = -bmpHeight;
  uint32_t rowSize = (bmpWidth * 3 + 3) & ~3;

  uint8_t sdbuffer[3 * 20];

  // Now switch to TFT for drawing
  digitalWrite(SD_CS, HIGH);
  delay(10);

  for (int row = 0; row < bmpHeight; row++) {
    // Switch to SD to read data
    digitalWrite(TFT_CS, HIGH);
    delay(1);

    bmpFile.seek(bmpImageOffset + (bmpHeight - 1 - row) * rowSize);

    int col = 0;
    while (col < bmpWidth) {
      int chunk = min(20, (int)(bmpWidth - col));
      bmpFile.read(sdbuffer, 3 * chunk);

      // Switch to TFT to draw
      digitalWrite(SD_CS, HIGH);
      delay(1);

      // Draw the pixels
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