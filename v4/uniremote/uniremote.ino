#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include <IRremote.hpp>
#include "./IR-codes.h"

// Display
#define TFT_CS 7
#define TFT_RST 10
#define TFT_DC 2
#define TFT_MOSI 6
#define TFT_MISO 5
#define TFT_CLK 4

// Display > Touch
#define TOUCH_CS 8
#define TOUCH_IRQ 9

// Display > SD
#define SD_CS 3

// IR Tx & Rx
#define IR_TX 0
#define IR_RX 1

// Touch calibration
#define TS_MINX 240
#define TS_MINY 250
#define TS_MAXX 3850
#define TS_MAXY 3750

// Initialize display & touch & SD
SPIClass spi(FSPI);
Adafruit_ILI9341 tft = Adafruit_ILI9341(&spi, TFT_DC, TFT_CS, TFT_RST);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

// Touchable button structure
struct TouchButton {
  int x, y, w, h;
  uint16_t color;
  uint16_t textColor;
  const char *label;
  void (*callback)();
  bool pressed;
};

// Helper variables
#define MAX_BUTTONS 10
TouchButton buttons[MAX_BUTTONS];
int buttonCount = 0;

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

// Global current theme colors
ThemeColors currentTheme;

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

// Default theme
#define DEFAULT_THEME THEME_FUTURISTIC_RED

// Functions
// Display helper
void initDisplay();
void processTouchButtons(int tx, int ty);
void drawButton(TouchButton *btn, bool active);
void drawHeaderFooter();
uint16_t read16(File &f);
uint32_t read32(File &f);
void drawBMP(const char *filename, int16_t x, int16_t y);
void createTouchBox(int x, int y, int width, int height, uint16_t color, uint16_t textColor, const char *label, void (*callback)());
bool isTouchInButton(TouchButton *btn, int tx, int ty);
void createOptions(const Option options[], int count, int x = 10, int y = 50, int btnWidth = 220, int btnHeight = 45);
void backToMenu();

// Signal options
void signalOptions();
void transmitOptions();
void receiveOptions();

// Projector brands
void projectorBrands();
void epsonOptions();
void acerOptions();
void benqOptions();
void necOptions();
void panasonicOptions();

// SD Card
void sdData();
void sdInfoOptions();
void listSDFiles();
void sdFormatOptions();

// Theme
void themeOptions();
void setTheme(uint8_t themeIndex);
void drawMenuUI();
void setThemeFuturisticRed();
void setThemeFuturisticGreen();
void setThemeFuturisticPurple();

// Menu options (first glance)
const Option MENU_OPTIONS[] = {
  { "Signal options", signalOptions },
  { "Projector brands", projectorBrands },
  { "SD Card options", sdData },
  { "Change theme", themeOptions }
};

// Signal options
const Option SIGNAL_OPTIONS[] = {
  { "Transmit", transmitOptions },
  { "Receive", receiveOptions },
  { "Back", backToMenu }
};

// Projector brands
const Option PROJECTOR_BRANDS[] = {
  { "EPSON", epsonOptions },
  { "ACER", acerOptions },
  { "BENQ", benqOptions },
  { "NEC", necOptions },
  { "PANASONIC", panasonicOptions },
  { "Back", backToMenu }
};

// SD Card
const Option SD_CARD_OPTIONS[] = {
  { "Info", sdInfoOptions },
  { "Files", listSDFiles },
  { "Format", sdFormatOptions },
  { "Back", backToMenu }
};

// Theme
const Option THEME_OPTIONS[] = {
  { "Futuristic Red", setThemeFuturisticRed },
  { "Futuristic Green", setThemeFuturisticGreen },
  { "Futuristic Purple", setThemeFuturisticPurple },
  { "Back", backToMenu }
};

void setup() {
  initDisplay();
}

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

  if (!SD.begin(SD_CS, spi)) {
    Serial.println("SD initialization failed!");
  }

  // Setup
  digitalWrite(SD_CS, HIGH);
  delay(10);

  // Draw UI and menu
  drawMenuUI();

  // Touch
  Serial.println("Initializing touch...");
  if (!touch.begin(spi)) {
    Serial.println("Touch FAILED!");
  }
  touch.setRotation(2);
}

// Draw header and footer decorations
void drawHeaderFooter() {
  // Header border
  tft.drawFastHLine(0, 0, 240, currentTheme.primary);
  tft.drawFastHLine(0, 1, 240, currentTheme.primary);

  // Top corner accents
  for (int i = 0; i < 15; i++) {
    tft.drawPixel(i, 2 + i / 3, currentTheme.primary);
    tft.drawPixel(239 - i, 2 + i / 3, currentTheme.primary);
  }

  // Footer border
  tft.drawFastHLine(0, 318, 240, currentTheme.primary);
  tft.drawFastHLine(0, 319, 240, currentTheme.primary);

  // Bottom corner accents
  for (int i = 0; i < 15; i++) {
    tft.drawPixel(i, 317 - i / 3, currentTheme.primary);
    tft.drawPixel(239 - i, 317 - i / 3, currentTheme.primary);
  }
}

// Draw a button in normal or active state
void drawButton(TouchButton *btn, bool active) {
  if (active) {
    // Active state - filled with primary color
    tft.fillRect(btn->x, btn->y, btn->w, btn->h, currentTheme.primary);
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
    tft.drawRect(btn->x, btn->y, btn->w, btn->h, currentTheme.primary);

    // Corner decorations
    int cornerSize = 8;
    tft.drawFastHLine(btn->x, btn->y, cornerSize, currentTheme.primary);
    tft.drawFastVLine(btn->x, btn->y, cornerSize, currentTheme.primary);
    tft.drawFastHLine(btn->x + btn->w - cornerSize, btn->y, cornerSize, currentTheme.primary);
    tft.drawFastVLine(btn->x + btn->w - 1, btn->y, cornerSize, currentTheme.primary);
    tft.drawFastHLine(btn->x, btn->y + btn->h - 1, cornerSize, currentTheme.primary);
    tft.drawFastVLine(btn->x, btn->y + btn->h - cornerSize, cornerSize, currentTheme.primary);
    tft.drawFastHLine(btn->x + btn->w - cornerSize, btn->y + btn->h - 1, cornerSize, currentTheme.primary);
    tft.drawFastVLine(btn->x + btn->w - 1, btn->y + btn->h - cornerSize, cornerSize, currentTheme.primary);

    // Add scanline effect
    for (int j = 2; j < btn->h - 2; j += 5) {
      tft.drawFastHLine(btn->x + 2, btn->y + j, btn->w - 4, currentTheme.darkest);
    }

    // Text in primary color
    tft.setTextColor(currentTheme.primary);
  }

  // Draw centered text
  tft.setTextSize(2);
  int16_t x1, y1;
  uint16_t w1, h1;
  tft.getTextBounds(btn->label, 0, 0, &x1, &y1, &w1, &h1);
  tft.setCursor(btn->x + (btn->w - w1) / 2, btn->y + (btn->h - h1) / 2);
  tft.print(btn->label);
}

void createOptions(const Option options[], int count, int x, int y, int btnWidth, int btnHeight) {
  buttonCount = 0;
  tft.fillRect(0, 60, 240, 258, ILI9341_BLACK);

  for (int i = 0; i < count; i++) {
    createTouchBox(x, y + 15 + (btnHeight + 5) * i, btnWidth, btnHeight, currentTheme.primary, currentTheme.primary, options[i].name, options[i].callback);
  }

  // Redraw footer
  drawHeaderFooter();
}

void signalOptions() {
  createOptions(SIGNAL_OPTIONS, 3, 10, 70);
}

void transmitOptions() {
}

void receiveOptions() {
}

void backToMenu() {
  createOptions(MENU_OPTIONS, 4, 10, 70);
}

void projectorBrands() {
  createOptions(PROJECTOR_BRANDS, 6, 10, 55, 220, 35);
}

void epsonOptions() {
}

void acerOptions() {
}

void benqOptions() {
}

void necOptions() {
}

void panasonicOptions() {
}

void sdData() {
  createOptions(SD_CARD_OPTIONS, 4, 10, 70);
}

void sdInfoOptions() {
}

void listSDFiles() {
}

void sdFormatOptions() {
}

void themeOptions() {
  createOptions(THEME_OPTIONS, 4, 10, 70);
}

// Theme callback functions
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
  Serial.print("Setting theme to: ");
  Serial.println(themeIndex);

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

  // Return to main menu
  backToMenu();
}

// Process touch for all buttons with futuristic feedback
void processTouchButtons(int tx, int ty) {
  for (int i = 0; i < buttonCount; i++) {
    TouchButton *btn = &buttons[i];

    if (isTouchInButton(btn, tx, ty)) {
      if (!btn->pressed) {
        btn->pressed = true;
        drawButton(btn, true);

        // Call the callback function
        if (btn->callback != NULL) {
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
  Serial.print("Loading BMP: ");
  Serial.println(filename);

  // Ensure TFT is deselected before accessing SD
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);
  delay(10);

  File bmpFile = SD.open(filename);
  if (!bmpFile) {
    Serial.println("File not found!");
    return;
  }
  Serial.println("File opened");

  // Verify BMP signature
  if (read16(bmpFile) != 0x4D42) {
    Serial.println("Not a BMP file");
    bmpFile.close();
    return;
  }
  Serial.println("Valid BMP signature");

  read32(bmpFile);  // skip file size
  read32(bmpFile);  // skip creator bytes
  uint32_t bmpImageOffset = read32(bmpFile);
  Serial.print("Image offset: ");
  Serial.println(bmpImageOffset);

  read32(bmpFile);  // DIB header size
  int32_t bmpWidth = read32(bmpFile);
  int32_t bmpHeight = read32(bmpFile);
  Serial.print("Image size: ");
  Serial.print(bmpWidth);
  Serial.print(" x ");
  Serial.println(bmpHeight);

  read16(bmpFile);  // color planes
  uint16_t bmpDepth = read16(bmpFile);
  read32(bmpFile);  // compression

  if (bmpDepth != 24) {
    Serial.print("Unsupported BMP depth: ");
    Serial.println(bmpDepth);
    bmpFile.close();
    return;
  }

  if (bmpHeight < 0)
    bmpHeight = -bmpHeight;
  uint32_t rowSize = (bmpWidth * 3 + 3) & ~3;
  Serial.print("Row size: ");
  Serial.println(rowSize);

  uint8_t sdbuffer[3 * 20];

  // Now switch to TFT for drawing
  digitalWrite(SD_CS, HIGH);
  delay(10);

  Serial.println("Starting to draw...");

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

    // Print progress every 10 rows
    if (row % 10 == 0) {
      Serial.print("Row: ");
      Serial.println(row);
    }
  }

  digitalWrite(TFT_CS, HIGH);
  bmpFile.close();
  Serial.println("BMP loaded successfully!");
}

// Futuristic button with scanlines and glowing effect
void createTouchBox(int x, int y, int width, int height, uint16_t color, uint16_t textColor, const char *label, void (*callback)()) {
  if (buttonCount >= MAX_BUTTONS) {
    Serial.println("Max buttons reached!");
    return;
  }

  TouchButton *btn = &buttons[buttonCount];
  btn->x = x;
  btn->y = y;
  btn->w = width;
  btn->h = height;
  btn->color = currentTheme.primary;
  btn->textColor = currentTheme.primary;
  btn->label = label;
  btn->callback = callback;
  btn->pressed = false;

  // Draw button in normal state
  drawButton(btn, false);

  buttonCount++;
}

// Check if a point is inside a button
bool isTouchInButton(TouchButton *btn, int tx, int ty) {
  return (tx >= btn->x && tx <= (btn->x + btn->w) && ty >= btn->y && ty <= (btn->y + btn->h));
}

void sendFixSignal(const uint16_t *signalData, size_t length) {
  IrSender.sendRaw(signalData, length, 38);
}