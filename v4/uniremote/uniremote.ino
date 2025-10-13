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

// ===================================================================================
// ================================= STRUCTS =========================================
// ===================================================================================

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
void receiveOptions();

// Projector brands
void projectorBrands();
void drawProjectorOptions(uint8_t brandIndex, const char *titleName, uint8_t titleXValue);
void epsonOptions();
void acerOptions();
void benqOptions();
void necOptions();
void panasonicOptions();

// SD Card
void sdData();
void listSDInfo();
String formatBytes(uint64_t bytes);
int countFilesInDirectory(const char *directoryPath);
void listSDFiles();
void sdFormatOptions();
bool deleteDirectory(const char *path);
void formatSD();

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
  { "Projector brands", projectorBrands },
  { "SD Card options", sdData },
  { "Change theme", themeOptions }
};

// Menu options (without SD card option)
const Option MENU_OPTIONS_NO_SD[] = {
  { "Signal options", signalOptions },
  { "Projector brands", projectorBrands },
  { "Change theme", themeOptions }
};

// Signal options
const Option SIGNAL_OPTIONS[] = {
  { "Transmit", transmitOptions },
  { "Receive", receiveOptions },
  { "Back", signalOptions }
};

const Option TRANSMIT_OPTIONS[] = {
  { "Saved signals", listSavedSignals },
  { "Favorites", listFavoriteSignals },
  { "Back", signalOptions }
};

const Option RECEIVE_OPTIONS[] = {
  { "Start listening", startSignalListen },
  { "Back", signalOptions }
};

// Projector brands
const Option PROJECTOR_BRANDS[] = {
  { "EPSON", epsonOptions },
  { "ACER", acerOptions },
  { "BENQ", benqOptions },
  { "NEC", necOptions },
  { "PANASONIC", panasonicOptions },
  { "Back", drawMenuUI }
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

// ===================================================================================
// ============================= INITIALIZATION ======================================
// ===================================================================================

// Initialize display & touch & SD
SPIClass spi(FSPI);
Adafruit_ILI9341 tft = Adafruit_ILI9341(&spi, TFT_DC, TFT_CS, TFT_RST);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
ThemeColors currentTheme;

// Helper variables
constexpr uint8_t MAX_BUTTONS = 10;
TouchButton buttons[MAX_BUTTONS];
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

  // Create 1x2 grid
  int btnSize = 100;
  int spacing = 10;
  int totalWidth = (btnSize * 2) + spacing;
  int startX = (240 - totalWidth) / 2;

  createTouchBox(startX, 70, btnSize, btnSize, currentTheme.primary, currentTheme.primary, "Transmit", transmitOptions);
  createTouchBox(startX + btnSize + spacing, 70, btnSize, btnSize, currentTheme.primary, currentTheme.primary, "Receive", receiveOptions);

  // Centered "Back" button
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
  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
  drawHeaderFooter();
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
  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
  drawHeaderFooter();
  drawTitle("Receive > Listen", 70);
}

void receiveOptions() {
  createOptions(RECEIVE_OPTIONS, 2, 10, 120);
  drawTitle("Signal > Receive", 70);
}

void projectorBrands() {
  buttonCount = 0;
  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);

  // 3x2 grid
  int btnWidth = 115;
  int btnHeight = 40;
  int spacing = 5;
  int startX = (240 - (btnWidth * 2 + spacing)) / 2;
  int startY = 120;

  int buttonIndex = 0;
  for (int row = 0; row < 3 && buttonIndex < 6; row++) {
    for (int col = 0; col < 2 && buttonIndex < 6; col++) {
      int x = startX + col * (btnWidth + spacing);
      int y = startY + row * (btnHeight + spacing);

      bool isBack = (buttonIndex == 5 && strcmp(PROJECTOR_BRANDS[buttonIndex].name, "Back") == 0);

      createTouchBox(x, y, btnWidth, btnHeight,
                     currentTheme.primary, currentTheme.primary,
                     PROJECTOR_BRANDS[buttonIndex].name, PROJECTOR_BRANDS[buttonIndex].callback, isBack);

      buttonIndex++;
    }
  }

  drawHeaderFooter();
  drawTitle("Projector brands", 70);
}

void drawProjectorOptions(uint8_t brandIndex, const char *titleName, uint8_t titleXValue) {
  const IRCode *selectedArr = nullptr;
  uint8_t selectedArrLength = 0;
  // Option extractedCodes[EPSON_CODES_LENGTH] = {};

  // titleXValue based on characters?
  // Select array by index
  switch (brandIndex) {
    // Epson
    case 0:
      selectedArr = EPSON_CODES;
      selectedArrLength = EPSON_CODES_LENGTH;
      break;
    // Acer
    case 1:
      selectedArr = ACER_CODES;
      selectedArrLength = ACER_CODES_LENGTH;
      break;
    // BenQ
    case 2:
      selectedArr = BENQ_CODES;
      selectedArrLength = BENQ_CODES_LENGTH;
      break;
    // NEC
    case 3:
      selectedArr = NEC_CODES;
      selectedArrLength = NEC_CODES_LENGTH;
      break;
    // Panasonic
    case 4:
      selectedArr = PANASONIC_CODES;
      selectedArrLength = PANASONIC_CODES_LENGTH;
      break;
    // Invalid index
    default:
      drawMenuUI();
      return;
  }

  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
  drawHeaderFooter();

  Serial.println(titleName);
  for (int i = 0; i < selectedArrLength; i++) {
    Serial.println(selectedArr[i].codeName);
    for (int j = 0; j < sizeof(selectedArr[i].codeArray) / sizeof(selectedArr[i].codeArray[0]); j++) {
      Serial.print(selectedArr[i].codeArray[j], HEX);
      Serial.print(" ");
    }
    Serial.println("\n------------------------------");
    // extractedCodes[i] = {EPSON_CODES[i].codeName, sendFixSignal(EPSON_CODES[i].codeArray)};
  }
  String fullTitle = "Projector > " + String(titleName);
  drawTitle(fullTitle.c_str(), titleXValue);
}

void epsonOptions() {
  drawProjectorOptions(0, "EPSON", 65);
}

void acerOptions() {
  drawProjectorOptions(1, "ACER", 70);
}

void benqOptions() {
  drawProjectorOptions(2, "BENQ", 70);
}

void necOptions() {
  drawProjectorOptions(3, "NEC", 75);
}

void panasonicOptions() {
  drawProjectorOptions(4, "PANASONIC", 55);
}

void sdData() {
  createOptions(SD_CARD_OPTIONS, 4, 10, 70);
  drawTitle("SD Card options", 75);
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
  if (bytes < 1024 * 1024 * 1024) {  // Less than 1 GB
    return String(bytes / (1024.0 * 1024.0), 1) + " MB";
  } else {
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
  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
  drawHeaderFooter();
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
    if (entryName == "System Volume Information" || entryName == "built-in-signals" || entryName == "/built-in-signals") {
      tft.setCursor(10, 130);
      tft.setTextColor(currentTheme.primary);
      tft.print("Skipping:");
      tft.setCursor(0, 160);
      tft.setTextColor(ILI9341_WHITE);
      tft.println(entryName);
      entry = root.openNextFile();
      delay(2000);
      continue;
    }

    String fullPath = "/" + entryName;

    tft.setCursor(10, 130);
    tft.setTextColor(currentTheme.primary);
    tft.print("Processing:");
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
      tft.setTextColor(currentTheme.primary);
      tft.println("Deleted:");
      tft.setCursor(0, 160);
      tft.setTextColor(ILI9341_WHITE);
      tft.println(entryName);
      delay(2000);
    } else {
      tft.setCursor(10, 130);
      tft.setTextColor(currentTheme.primary);
      tft.println("Failed to delete:");
      tft.setCursor(0, 160);
      tft.setTextColor(ILI9341_WHITE);
      tft.println(entryName);
      delay(2000);
    }

    entry = root.openNextFile();
  }

  root.close();

  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);
  tft.setTextColor(currentTheme.primary);
  tft.setTextSize(2);
  tft.setCursor(20, 140);
  tft.println("Formatting done!");
  delay(2000);
  drawMenuUI();
}

void themeOptions() {
  createOptions(THEME_OPTIONS, 4, 10, 70);
  drawTitle("Change theme", 85);
}

// ===================================================================================
// ===================================== THEMES ======================================
// ===================================================================================

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
  if (buttonCount >= MAX_BUTTONS) {
    Serial.println("Max buttons reached!");
    return;
  }

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
  }

  // Draw centered text
  tft.setTextSize(2);
  int16_t x1, y1;
  uint16_t w1, h1;
  tft.getTextBounds(btn->label, 0, 0, &x1, &y1, &w1, &h1);
  tft.setCursor(btn->x + (btn->w - w1) / 2, btn->y + (btn->h - h1) / 2);
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