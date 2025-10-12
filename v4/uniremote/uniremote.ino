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

// Color palette - Red/Black theme
#define COLOR_BLACK      0x0000
#define COLOR_DARKGREY   0x2104
#define COLOR_GREY       0x4208
#define COLOR_RED        0xF800
#define COLOR_DARKRED    0x8800
#define COLOR_CRIMSON    0xD000
#define COLOR_WHITE      0xFFFF

// Helper variables
#define MAX_BUTTONS 10
TouchButton buttons[MAX_BUTTONS];
int buttonCount = 0;

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

// Option structure with name and callback
struct Option {
  const char *name;
  void (*callback)();
};

const Option options[] = {
  { "Signal options", showSignalOptions },
  { "Projector brands", showProjectorBrands },
  { "SD Card", showSDData },
};

// Functions
void showSignalOptions();
void showProjectorBrands();
void showSDData();
void processTouchButtons(int tx, int ty);
uint16_t read16(File &f);
uint16_t read32(File &f);
void drawBMP(const char *filename, int16_t x, int16_t y);
void createTouchBox(int x, int y, int width, int height, uint16_t color, uint16_t textColor, const char *label, void (*callback)());
bool isTouchInButton(TouchButton *btn, int tx, int ty);
void epsonFreeze();
void acerFreeze();
void benqFreeze();
void necFreeze();
void panasonicFreeze();

void setup() {
  Serial.begin(115200);

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
  tft.fillScreen(ILI9341_BLACK);

  // SD
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);

  if (!SD.begin(SD_CS, spi)) {
    Serial.println("SD initialization failed!");
  } else {
    Serial.println("SD initialization done!");
    // List files to verify
    File root = SD.open("/");
    if (root) {
      File entry;
      Serial.println("Files on SD:");
      while ((entry = root.openNextFile())) {
        Serial.print("  ");
        Serial.println(entry.name());
        entry.close();
      }
    }
  }

  // Setup
  digitalWrite(SD_CS, HIGH);
  delay(10);

  // Futuristic header design
  // Top border
  tft.drawFastHLine(0, 0, 240, COLOR_RED);
  tft.drawFastHLine(0, 1, 240, COLOR_RED);
  
  // Corner accents
  for (int i = 0; i < 15; i++) {
    tft.drawPixel(i, 2 + i/3, COLOR_RED);
    tft.drawPixel(239 - i, 2 + i/3, COLOR_RED);
  }
  
  // Main title
  tft.setTextColor(COLOR_RED);
  tft.setTextSize(3);
  tft.setCursor(40, 10);
  tft.println("PROJECTOR");
  
  // Subtitle with lines
  tft.drawFastHLine(15, 38, 210, COLOR_DARKRED);
  tft.setTextSize(1);
  tft.setCursor(75, 42);
  tft.println("REMOTE CONTROL");
  tft.drawFastHLine(15, 52, 210, COLOR_DARKRED);

  // Create futuristic buttons
  for (int i = 0; i < sizeof(options) / sizeof(options[0]); i++) {
    createTouchBox(10, 65 + (50 * i), 220, 45, COLOR_RED, COLOR_RED, 
                   options[i].name, options[i].callback);
  }

  // Touch
  Serial.println("Initializing touch...");
  if (touch.begin(spi)) {
    Serial.println("Touch OK!");
  } else {
    Serial.println("Touch FAILED!");
  }
  touch.setRotation(2);

  Serial.println("=== Setup Complete ===\n");
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
        TouchButton *btn = &buttons[i];
        TouchButton *btn = &buttons[i];
        
        tft.drawRect(btn->x - 1, btn->y - 1, btn->w + 2, btn->h + 2, COLOR_DARKRED);
        tft.drawRect(btn->x - 2, btn->y - 2, btn->w + 4, btn->h + 4, COLOR_DARKGREY);
        tft.fillRect(btn->x, btn->y, btn->w, btn->h, COLOR_BLACK);
        tft.drawRect(btn->x, btn->y, btn->w, btn->h, COLOR_RED);
        
        int cornerSize = 8;
        tft.drawFastHLine(btn->x, btn->y, cornerSize, COLOR_RED);
        tft.drawFastVLine(btn->x, btn->y, cornerSize, COLOR_RED);
        tft.drawFastHLine(btn->x + btn->w - cornerSize, btn->y, cornerSize, COLOR_RED);
        tft.drawFastVLine(btn->x + btn->w - 1, btn->y, cornerSize, COLOR_RED);
        tft.drawFastHLine(btn->x, btn->y + btn->h - 1, cornerSize, COLOR_RED);
        tft.drawFastVLine(btn->x, btn->y + btn->h - cornerSize, cornerSize, COLOR_RED);
        tft.drawFastHLine(btn->x + btn->w - cornerSize, btn->y + btn->h - 1, cornerSize, COLOR_RED);
        tft.drawFastVLine(btn->x + btn->w - 1, btn->y + btn->h - cornerSize, cornerSize, COLOR_RED);
        
        for (int j = 2; j < btn->h - 2; j += 3) {
          tft.drawFastHLine(btn->x + 2, btn->y + j, btn->w - 4, COLOR_DARKGREY);
        }
        
        tft.setTextSize(2);
        int16_t x1, y1;
        uint16_t w1, h1;
        tft.getTextBounds(btn->label, 0, 0, &x1, &y1, &w1, &h1);
        tft.setCursor(btn->x + (btn->w - w1) / 2, btn->y + (btn->h - h1) / 2);
        tft.print(btn->label);
      }
    }
  }

  delay(10);
}

void showSignalOptions(){

}

void showProjectorBrands(){

}

void showSDData(){

}

// Process touch for all buttons with futuristic feedback
void processTouchButtons(int tx, int ty) {
  for (int i = 0; i < buttonCount; i++) {
    TouchButton *btn = &buttons[i];

    if (isTouchInButton(btn, tx, ty)) {
      if (!btn->pressed) {
        btn->pressed = true;

        // Active state
        tft.fillRect(btn->x, btn->y, btn->w, btn->h, COLOR_RED);
        tft.drawRect(btn->x, btn->y, btn->w, btn->h, COLOR_WHITE);
        
        // Corner accents in white
        int cornerSize = 8;
        tft.drawFastHLine(btn->x, btn->y, cornerSize, COLOR_WHITE);
        tft.drawFastVLine(btn->x, btn->y, cornerSize, COLOR_WHITE);
        tft.drawFastHLine(btn->x + btn->w - cornerSize, btn->y, cornerSize, COLOR_WHITE);
        tft.drawFastVLine(btn->x + btn->w - 1, btn->y, cornerSize, COLOR_WHITE);
        tft.drawFastHLine(btn->x, btn->y + btn->h - 1, cornerSize, COLOR_WHITE);
        tft.drawFastVLine(btn->x, btn->y + btn->h - cornerSize, cornerSize, COLOR_WHITE);
        tft.drawFastHLine(btn->x + btn->w - cornerSize, btn->y + btn->h - 1, cornerSize, COLOR_WHITE);
        tft.drawFastVLine(btn->x + btn->w - 1, btn->y + btn->h - cornerSize, cornerSize, COLOR_WHITE);

        // Text in black
        tft.setTextColor(COLOR_BLACK);
        tft.setTextSize(2);
        int16_t x1, y1;
        uint16_t w1, h1;
        tft.getTextBounds(btn->label, 0, 0, &x1, &y1, &w1, &h1);
        tft.setCursor(btn->x + (btn->w - w1) / 2, btn->y + (btn->h - h1) / 2);
        tft.print(btn->label);

        // Call the callback function
        if (btn->callback != NULL) {
          btn->callback();
        }
      }
    } else {
      if (btn->pressed) {
        btn->pressed = false;
        
        // Redraw in normal state
        tft.drawRect(btn->x - 1, btn->y - 1, btn->w + 2, btn->h + 2, COLOR_DARKRED);
        tft.drawRect(btn->x - 2, btn->y - 2, btn->w + 4, btn->h + 4, COLOR_DARKGREY);
        tft.fillRect(btn->x, btn->y, btn->w, btn->h, COLOR_BLACK);
        tft.drawRect(btn->x, btn->y, btn->w, btn->h, COLOR_RED);
        
        int cornerSize = 8;
        tft.drawFastHLine(btn->x, btn->y, cornerSize, COLOR_RED);
        tft.drawFastVLine(btn->x, btn->y, cornerSize, COLOR_RED);
        tft.drawFastHLine(btn->x + btn->w - cornerSize, btn->y, cornerSize, COLOR_RED);
        tft.drawFastVLine(btn->x + btn->w - 1, btn->y, cornerSize, COLOR_RED);
        tft.drawFastHLine(btn->x, btn->y + btn->h - 1, cornerSize, COLOR_RED);
        tft.drawFastVLine(btn->x, btn->y + btn->h - cornerSize, cornerSize, COLOR_RED);
        tft.drawFastHLine(btn->x + btn->w - cornerSize, btn->y + btn->h - 1, cornerSize, COLOR_RED);
        tft.drawFastVLine(btn->x + btn->w - 1, btn->y + btn->h - cornerSize, cornerSize, COLOR_RED);
        
        for (int j = 2; j < btn->h - 2; j += 3) {
          tft.drawFastHLine(btn->x + 2, btn->y + j, btn->w - 4, COLOR_DARKGREY);
        }
        
        tft.setTextColor(COLOR_RED);
        tft.setTextSize(2);
        int16_t x1, y1;
        uint16_t w1, h1;
        tft.getTextBounds(btn->label, 0, 0, &x1, &y1, &w1, &h1);
        tft.setCursor(btn->x + (btn->w - w1) / 2, btn->y + (btn->h - h1) / 2);
        tft.print(btn->label);
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
void createTouchBox(int x, int y, int width, int height,
                    uint16_t color, uint16_t textColor,
                    const char *label, void (*callback)()) {
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

  // Draw outer glow
  tft.drawRect(x - 1, y - 1, width + 2, height + 2, COLOR_DARKRED);
  tft.drawRect(x - 2, y - 2, width + 4, height + 4, COLOR_DARKGREY);
  
  // Main button background
  tft.fillRect(x, y, width, height, COLOR_BLACK);
  
  // Red border with corner accents
  tft.drawRect(x, y, width, height, COLOR_RED);
  
  // Corner decorations
  int cornerSize = 8;
  // Top-left
  tft.drawFastHLine(x, y, cornerSize, COLOR_RED);
  tft.drawFastVLine(x, y, cornerSize, COLOR_RED);
  // Top-right
  tft.drawFastHLine(x + width - cornerSize, y, cornerSize, COLOR_RED);
  tft.drawFastVLine(x + width - 1, y, cornerSize, COLOR_RED);
  // Bottom-left
  tft.drawFastHLine(x, y + height - 1, cornerSize, COLOR_RED);
  tft.drawFastVLine(x, y + height - cornerSize, cornerSize, COLOR_RED);
  // Bottom-right
  tft.drawFastHLine(x + width - cornerSize, y + height - 1, cornerSize, COLOR_RED);
  tft.drawFastVLine(x + width - 1, y + height - cornerSize, cornerSize, COLOR_RED);
  
  // Add scanline effect
  for (int i = 2; i < height - 2; i += 3) {
    tft.drawFastHLine(x + 2, y + i, width - 4, COLOR_DARKGREY);
  }

  // Draw centered text
  tft.setTextColor(COLOR_RED);
  tft.setTextSize(2);
  
  int16_t x1, y1;
  uint16_t w1, h1;
  tft.getTextBounds(label, 0, 0, &x1, &y1, &w1, &h1);
  tft.setCursor(x + (width - w1) / 2, y + (height - h1) / 2);
  tft.print(label);

  buttonCount++;
}

// Check if a point is inside a button
bool isTouchInButton(TouchButton *btn, int tx, int ty) {
  return (tx >= btn->x && tx <= (btn->x + btn->w) && ty >= btn->y && ty <= (btn->y + btn->h));
}

void sendFixSignal(const uint16_t *signalData, size_t length) {
  IrSender.sendRaw(signalData, length, 38);
}

void epsonFreeze() {
  sendFixSignal(EPSON_CODES[0], sizeof(EPSON_CODES[0]) / sizeof(EPSON_CODES[0][0]));
}

void acerFreeze() {
  sendFixSignal(ACER_CODES[0], sizeof(ACER_CODES[0]) / sizeof(ACER_CODES[0][0]));
}

void benqFreeze() {
  sendFixSignal(BENQ_CODES[0], sizeof(BENQ_CODES[0]) / sizeof(BENQ_CODES[0][0]));
}

void necFreeze() {
  sendFixSignal(NEC_CODES[0], sizeof(NEC_CODES[0]) / sizeof(NEC_CODES[0][0]));
}

void panasonicFreeze() {
  sendFixSignal(PANASONIC_CODES[0], sizeof(PANASONIC_CODES[0]) / sizeof(PANASONIC_CODES[0][0]));
}