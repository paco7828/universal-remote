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

// Brand structure with name and callback
struct Brand {
  const char *name;
  void (*callback)();
};

const Brand brands[] = {
  { "EPSON", epsonFreeze },
  { "ACER", acerFreeze },
  { "BENQ", benqFreeze },
  { "NEC", necFreeze },
  { "PANASONIC", panasonicFreeze }
};

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

  uint8_t sdbuffer[3 * 20];  // buffer 20 pixels at a time

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

// Function to create a touchable box
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

  // Draw the button
  tft.fillRoundRect(x, y, width, height, 8, color);
  tft.drawRoundRect(x, y, width, height, 8, ILI9341_WHITE);

  // Draw label centered
  tft.setTextColor(textColor);
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

// Process touch for all buttons
void processTouchButtons(int tx, int ty) {
  for (int i = 0; i < buttonCount; i++) {
    TouchButton *btn = &buttons[i];

    if (isTouchInButton(btn, tx, ty)) {
      if (!btn->pressed) {
        btn->pressed = true;

        // Visual feedback - darken button
        uint16_t darkColor = tft.color565(
          (((btn->color >> 11) & 0x1F) * 6) / 10,
          (((btn->color >> 5) & 0x3F) * 6) / 10,
          ((btn->color & 0x1F) * 6) / 10);
        tft.fillRoundRect(btn->x, btn->y, btn->w, btn->h, 8, darkColor);

        // Redraw label
        tft.setTextColor(btn->textColor);
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
        // Redraw button in normal state
        tft.fillRoundRect(btn->x, btn->y, btn->w, btn->h, 8, btn->color);
        tft.drawRoundRect(btn->x, btn->y, btn->w, btn->h, 8, ILI9341_WHITE);
        tft.setTextColor(btn->textColor);
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
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 10);
  tft.println("Projector test");

  for (int i = 0; i < sizeof(brands) / sizeof(brands[0]); i++) {
    createTouchBox(15, 50 + (50 * i), 200, 40, ILI9341_RED, ILI9341_WHITE, brands[i].name, brands[i].callback);
  }

  // Touch
  Serial.println("Initializing touch...");
  if (touch.begin(spi)) {
    Serial.println("Touch initialized!");
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
        tft.fillRoundRect(btn->x, btn->y, btn->w, btn->h, 8, btn->color);
        tft.drawRoundRect(btn->x, btn->y, btn->w, btn->h, 8, ILI9341_WHITE);
        tft.setTextColor(btn->textColor);
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