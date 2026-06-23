#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <SPI.h>
#include <SD.h>
#include <IRremote.hpp>
#include <Preferences.h>
#include <functional>
#include "./IR-codes.h"

// ============================================================
// Pin definitions
// ============================================================
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

// ============================================================
// Display driver
// ============================================================
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
      cfg.pin_miso = -1;
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

// ============================================================
// Structs & type aliases
// ============================================================
constexpr int MAX_SAVED_SIGNAL_CHARS = 25;

struct IRSignal {
  char name[MAX_SAVED_SIGNAL_CHARS + 1];
  uint16_t rawData[200];
  uint8_t rawDataLen;
} __attribute__((packed));

struct TouchButton {
  int x, y, w, h;
  uint16_t color, textColor;
  const char *label;
  void (*callback)();
  bool pressed, isBackButton, repeatable;
};

using RowRenderer = std::function<void(int idx, int y, int rowH, bool selected)>;

struct ScrollList {
  int itemCount = 0;
  int rowHeight = 32;
  int selectedIndex = -1;
  float scrollPx = 0;
  int viewX = 0, viewY = 0, viewW = 0, viewH = 0;
  RowRenderer renderRow;
  std::function<void()> onOpen = nullptr;
};

struct Option {
  const char *name;
  void (*callback)();
};

struct ThemeColors {
  uint16_t primary, secondary, accent, dark, darkest;
};

struct HardcodedBrand {
  const char *brandName;
  const IRCode *codes;
  uint8_t codesLength;
};

// ============================================================
// Constants
// ============================================================
// Scroll list viewport — shared by every list screen
constexpr int LIST_VIEW_X = 5;
constexpr int LIST_VIEW_Y = 26;
constexpr int LIST_VIEW_W = 230;
constexpr int LIST_VIEW_H = 228;
constexpr int LIST_BUTTON_Y = 260;

// Touch timing
constexpr unsigned long REPEAT_INTERVAL = 200;
constexpr int SCROLL_DRAG_THRESHOLD = 10;
constexpr unsigned long DOUBLE_TAP_WINDOW = 400;

// ============================================================
// Theme presets
// ============================================================
const ThemeColors THEME_FUTURISTIC_RED = { 0xF800, 0xD000, 0x8800, 0x4208, 0x2104 };
const ThemeColors THEME_FUTURISTIC_GREEN = { 0x07E0, 0x05C0, 0x0400, 0x4208, 0x2104 };
const ThemeColors THEME_FUTURISTIC_PURPLE = { 0xF81F, 0xC81F, 0x8010, 0x4208, 0x2104 };

// ============================================================
// Static data (menus & brand library)
// ============================================================
void signalOptions();
void listSavedSignals();
void startSignalListen();
void builtInSignalsBrowser();
void sdData();
void themeOptions();
void drawMenuUI();
void listSDInfo();
void listSDFiles();
void sdFormatOptions();
void formatSD();
void setThemeFuturisticRed();
void setThemeFuturisticGreen();
void setThemeFuturisticPurple();

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
const Option THEME_OPTIONS[] = {
  { "Futuristic Red", setThemeFuturisticRed },
  { "Futuristic Green", setThemeFuturisticGreen },
  { "Futuristic Purple", setThemeFuturisticPurple },
  { "Back", drawMenuUI }
};

const HardcodedBrand hardcodedBrands[] = {
  { "EPSON", EPSON_CODES, EPSON_CODES_LENGTH },
  { "LED_STRIP", LED_STRIP_CODES, LED_STRIP_CODES_LENGTH },
  { "ACER", ACER_CODES, ACER_CODES_LENGTH },
  { "BENQ", BENQ_CODES, BENQ_CODES_LENGTH },
  { "NEC", NEC_CODES, NEC_CODES_LENGTH },
  { "PANASONIC", PANASONIC_CODES, PANASONIC_CODES_LENGTH }
};
const uint8_t hardcodedBrandsLength = sizeof(hardcodedBrands) / sizeof(hardcodedBrands[0]);

// ============================================================
// Global state
// ============================================================
// --- Display & preferences ---
LGFX tft;
SPIClass spiSD(FSPI);
ThemeColors currentTheme;
Preferences prefs;
bool initializedSD = false;

// --- Scroll list engine ---
LGFX_Sprite listSprite(&tft);
bool listSpriteReady = false;
ScrollList activeList;
ScrollList *activeScrollList = nullptr;

// --- Button system ---
TouchButton buttons[30];
uint8_t buttonCount = 0;
uint8_t activeBtnIndex = 0;

// --- IR capture ---
uint16_t currentRawData[200];
uint32_t currentDecodedHex = 0;
uint8_t currentRawDataLen = 0;
bool signalCaptured = false;
bool listeningForSignal = false;

// --- Built-in signal browser ---
const IRCode *currentBrandCodes = nullptr;
uint8_t currentBrandCodesLength = 0;
int builtInBrandCount = 0;
int builtInSignalCount = 0;
String currentBrandPath = "";

// --- SD file browser ---
String sdFiles[500];
int sdFileCount = 0;
String currentPath = "/";

// --- Saved signal groups ---
String savedSignalGroups[50];
int savedSignalGroupCount = 0;
String currentSavedGroup = "";
String groupedSignalFiles[50];
int groupedSignalCount = 0;

// --- Keyboard ---
const char *const qwerty0[10] = { "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P" };
const char *const qwerty1[9] = { "A", "S", "D", "F", "G", "H", "J", "K", "L" };
const char *const qwerty2[7] = { "Z", "X", "C", "V", "B", "N", "M" };
char outputText[MAX_SAVED_SIGNAL_CHARS + 1] = "";

// --- Touch / gesture ---
bool touchHeld = false;
int heldButtonIndex = -1;
unsigned long lastRepeatFire = 0;
bool scrollGestureActive = false;
bool scrollIsDragging = false;
int32_t scrollStartY = 0;
float scrollStartPx = 0;
int lastTapIndex = -1;
unsigned long lastTapTime = 0;

// ============================================================
// Function prototypes
// ============================================================
// Display
void initDisplay();
void drawBootSplash();
void drawMenuUI();
void clearScreen();
void drawHeaderFooter();
void drawTitle(const char *title, uint16_t x = 90, uint16_t y = 305);
void drawBackBtn(uint8_t x, uint8_t y, uint8_t w, uint8_t h, void (*cb)());
void printCentered(const char *text, int y, uint16_t color, uint8_t size);

// Scroll engine
void ensureListSprite(int w, int h);
void clampScroll(ScrollList &list);
void renderScrollList(ScrollList &list);
void setupAndRenderScrollList(int count, int rowH, RowRenderer renderer);

// Touch system
int processTouchButtons(int tx, int ty);
bool isTouchInButton(TouchButton *btn, int tx, int ty);
void drawButton(TouchButton *btn, bool active);
void createTouchBox(int x, int y, int w, int h, uint16_t color, uint16_t textColor, const char *label, void (*cb)(), bool isBack = false, bool repeatable = false);
void createOptions(const Option opts[], int count, int x = 10, int y = 50, int bw = 220, int bh = 45);

// Screens
void signalOptions();
void listSavedSignals();
void drawSavedSignalsList();
void startSignalListen();
void listBuiltInSignals();
void builtInSignalsBrowser();
void sdData();
void listSDInfo();
void listSDFiles();
void loadSDFiles(String path);
void drawSDFileBrowser();
void sdFormatOptions();
void formatSD();
void listGroupedSignals();
void drawGroupedSignalsList();
void deleteSelectedFile();
void themeOptions();

// Keyboard
void drawKeyboard();
void keyboardButtonPressed();

// IR
void captureSignal();
void saveSignalToSD(const IRSignal &signal);
void transmitSignal(const IRSignal &signal);
uint16_t prontoToRawSignal(const uint16_t *pronto, uint16_t *outRaw, uint8_t maxOut);

// SD helpers
String formatBytes(uint64_t bytes);
int countFilesInDirectory(const char *path);
bool deleteDirectory(const char *path);
void formatStatusLine(const char *label, uint16_t labelColor, const String &name);
String extractPrefix(String filename);

// Theme
ThemeColors themeFromIndex(uint8_t idx);
void setTheme(uint8_t themeIndex);
void setThemeFuturisticRed();
void setThemeFuturisticGreen();
void setThemeFuturisticPurple();

// ============================================================
// Setup & loop
// ============================================================
void setup() {
  initDisplay();
}

bool pointInScrollView(ScrollList *list, int x, int y) {
  return list
         && x >= list->viewX && x <= list->viewX + list->viewW
         && y >= list->viewY && y <= list->viewY + list->viewH;
}

void loop() {
  if (listeningForSignal && !signalCaptured) {
    if (IrReceiver.decode()) {
      captureSignal();
      buttonCount = 0;
      clearScreen();
      if (currentRawDataLen < 10) {
        printCentered("Invalid", 120, currentTheme.primary, 2);
        printCentered("signal!", 140, currentTheme.primary, 2);
        delay(2000);
        startSignalListen();
      } else {
        signalCaptured = true;
        listeningForSignal = false;
        printCentered("Captured!", 150, currentTheme.primary, 2);
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
      if (pointInScrollView(activeScrollList, (int)tx, (int)ty)) {
        scrollGestureActive = true;
        scrollIsDragging = false;
        scrollStartY = ty;
        scrollStartPx = activeScrollList->scrollPx;
        heldButtonIndex = -1;
      } else {
        scrollGestureActive = false;
        heldButtonIndex = processTouchButtons((int)tx, (int)ty);
        lastRepeatFire = millis();
      }
    } else if (scrollGestureActive && activeScrollList) {
      int32_t delta = ty - scrollStartY;
      if (!scrollIsDragging && abs((int)delta) > SCROLL_DRAG_THRESHOLD)
        scrollIsDragging = true;
      if (scrollIsDragging) {
        activeScrollList->scrollPx = scrollStartPx - delta;
        clampScroll(*activeScrollList);
        renderScrollList(*activeScrollList);
      }
    } else if (heldButtonIndex >= 0 && heldButtonIndex < buttonCount) {
      TouchButton *btn = &buttons[heldButtonIndex];
      if (btn->repeatable && isTouchInButton(btn, (int)tx, (int)ty)) {
        unsigned long now = millis();
        if (now - lastRepeatFire > REPEAT_INTERVAL) {
          lastRepeatFire = now;
          if (btn->callback) btn->callback();
          if (heldButtonIndex < buttonCount) {
            buttons[heldButtonIndex].pressed = true;
            drawButton(&buttons[heldButtonIndex], true);
          }
        }
      }
    }
    delay(scrollIsDragging ? 16 : 50);

  } else {
    if (scrollGestureActive && !scrollIsDragging && activeScrollList) {
      int relY = scrollStartY - activeScrollList->viewY;
      int tapped = (int)((activeScrollList->scrollPx + relY) / activeScrollList->rowHeight);
      if (tapped >= 0 && tapped < activeScrollList->itemCount) {
        unsigned long now = millis();
        bool isDoubleTap = (tapped == lastTapIndex) && (now - lastTapTime < DOUBLE_TAP_WINDOW);
        activeScrollList->selectedIndex = tapped;
        renderScrollList(*activeScrollList);
        if (isDoubleTap && activeScrollList->onOpen) {
          lastTapIndex = -1;
          activeScrollList->onOpen();
        } else {
          lastTapIndex = tapped;
          lastTapTime = now;
        }
      }
    }
    scrollGestureActive = false;
    scrollIsDragging = false;
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

// ============================================================
// Display
// ============================================================
void initDisplay() {
  Serial.begin(115200);
  prefs.begin("uniremote", true);
  currentTheme = themeFromIndex(prefs.getUChar("theme", 0));
  prefs.end();

  IrSender.begin(IR_TX);
  IrReceiver.begin(IR_RX);

  for (uint8_t cs : { TFT_CS, TOUCH_CS, SD_CS }) {
    pinMode(cs, OUTPUT);
    digitalWrite(cs, HIGH);
  }

  spiSD.begin(TFT_CLK, TFT_MISO, TFT_MOSI, SD_CS);
  initializedSD = SD.begin(SD_CS, spiSD, 20000000);
  if (initializedSD && !SD.exists("/saved-signals")) SD.mkdir("/saved-signals");

  tft.init();
  tft.setRotation(0);
  drawBootSplash();
}

void clearScreen() {
  tft.fillRect(0, 10, 240, 308, TFT_BLACK);
  drawHeaderFooter();
}

void drawHeaderFooter() {
  activeScrollList = nullptr;
  activeList.onOpen = nullptr;
  lastTapIndex = -1;
  tft.drawFastHLine(0, 0, 239, currentTheme.primary);
  for (int i = 0; i < 15; i++) {
    tft.drawPixel(i, 2 + i / 3, currentTheme.primary);
    tft.drawPixel(239 - i, 2 + i / 3, currentTheme.primary);
  }
  tft.drawFastHLine(0, 318, 240, currentTheme.primary);
  for (int i = 0; i < 15; i++) {
    tft.drawPixel(i, 317 - i / 3, currentTheme.primary);
    tft.drawPixel(239 - i, 317 - i / 3, currentTheme.primary);
  }
}

void drawTitle(const char *title, uint16_t x, uint16_t y) {
  tft.setTextSize(1);
  tft.setTextColor(currentTheme.primary);
  tft.setCursor(x, y);
  tft.println(title);
}

void printCentered(const char *text, int y, uint16_t color, uint8_t size) {
  tft.setTextSize(size);
  tft.setTextColor(color);
  tft.setCursor((240 - tft.textWidth(text)) / 2, y);
  tft.print(text);
}

void drawBackBtn(uint8_t x, uint8_t y, uint8_t w, uint8_t h, void (*cb)()) {
  createTouchBox(x, y, w, h, currentTheme.secondary, currentTheme.secondary, "Back", cb, true);
}

void drawBootSplash() {
  tft.fillScreen(TFT_BLACK);
  drawHeaderFooter();
  printCentered("UNIVERSAL", 120, currentTheme.primary, 3);
  printCentered("REMOTE", 155, currentTheme.primary, 3);
  delay(2000);
  drawMenuUI();
}

void drawMenuUI() {
  tft.fillScreen(TFT_BLACK);
  drawHeaderFooter();
  if (initializedSD) {
    createOptions(MENU_OPTIONS, 4, 10, 27, 220, 52);
  } else {
    createOptions(MENU_OPTIONS_NO_SD, 3, 10, 40, 220, 62);
  }
  drawTitle("MENU", 110);
}

// ============================================================
// Scroll engine
// ============================================================
void ensureListSprite(int w, int h) {
  if (listSpriteReady) return;
  listSprite.setColorDepth(16);
  if (listSprite.createSprite(w, h)) {
    listSpriteReady = true;
  } else {
    Serial.println("Sprite alloc failed — not enough RAM");
  }
}

void clampScroll(ScrollList &list) {
  float maxScroll = max(0.0f, (float)(list.itemCount * list.rowHeight - list.viewH));
  list.scrollPx = constrain(list.scrollPx, 0.0f, maxScroll);
}

void renderScrollList(ScrollList &list) {
  ensureListSprite(list.viewW, list.viewH);
  listSprite.fillSprite(TFT_BLACK);

  int firstIndex = (int)(list.scrollPx / list.rowHeight);
  int subPixel = (int)list.scrollPx - firstIndex * list.rowHeight;
  int rowsToDraw = list.viewH / list.rowHeight + 2;

  for (int n = 0; n < rowsToDraw; n++) {
    int idx = firstIndex + n;
    if (idx < 0 || idx >= list.itemCount) continue;
    int y = n * list.rowHeight - subPixel;
    if (y + list.rowHeight < 0 || y > list.viewH) continue;
    if (list.renderRow) list.renderRow(idx, y, list.rowHeight, idx == list.selectedIndex);
  }

  float maxScroll = (float)(list.itemCount * list.rowHeight - list.viewH);
  if (maxScroll > 0) {
    int barH = max(15, (int)(list.viewH * (float)list.viewH / (list.itemCount * list.rowHeight)));
    int barY = (int)((list.viewH - barH) * (list.scrollPx / maxScroll));
    listSprite.fillRect(list.viewW - 3, barY, 3, barH, currentTheme.secondary);
  }
  listSprite.pushSprite(list.viewX, list.viewY);
}

void setupAndRenderScrollList(int count, int rowH, RowRenderer renderer) {
  activeList.itemCount = count;
  activeList.rowHeight = rowH;
  activeList.viewX = LIST_VIEW_X;
  activeList.viewY = LIST_VIEW_Y;
  activeList.viewW = LIST_VIEW_W;
  activeList.viewH = LIST_VIEW_H;
  activeList.renderRow = renderer;
  clampScroll(activeList);
  activeScrollList = &activeList;
  renderScrollList(activeList);
}

// ============================================================
// Screens — Signal
// ============================================================
void signalOptions() {
  buttonCount = 0;
  clearScreen();
  const int btnSize = 100, gap = 10;
  const int startX = (240 - btnSize * 2 - gap) / 2;
  createTouchBox(startX, 70, btnSize, btnSize, currentTheme.primary, currentTheme.primary, "Transmit", listSavedSignals);
  createTouchBox(startX + btnSize + gap, 70, btnSize, btnSize, currentTheme.primary, currentTheme.primary, "Receive", startSignalListen);
  createTouchBox(60, 190, 120, 45, currentTheme.secondary, currentTheme.secondary, "Back", drawMenuUI, true);
  drawHeaderFooter();
  drawTitle("Signal options", 80);
}

void listSavedSignals() {
  savedSignalGroupCount = 0;
  File dir = SD.open("/saved-signals");
  if (dir) {
    for (File e = dir.openNextFile(); e && savedSignalGroupCount < 50; e = dir.openNextFile()) {
      if (!e.isDirectory()) {
        String prefix = extractPrefix(String(e.name()));
        bool found = false;
        for (int i = 0; i < savedSignalGroupCount; i++) {
          if (savedSignalGroups[i] == prefix) {
            found = true;
            break;
          }
        }
        if (!found) savedSignalGroups[savedSignalGroupCount++] = prefix;
      }
      e.close();
    }
    dir.close();
  }
  activeList.selectedIndex = 0;
  activeList.scrollPx = 0;
  drawSavedSignalsList();
}

void drawSavedSignalsList() {
  buttonCount = 0;
  clearScreen();
  if (savedSignalGroupCount == 0) {
    printCentered("No signals", 150, currentTheme.primary, 2);
    printCentered("saved!", 170, currentTheme.primary, 2);
    drawBackBtn(60, 200, 120, 40, signalOptions);
    drawTitle("Transmit > Saved", 70);
    return;
  }
  setupAndRenderScrollList(savedSignalGroupCount, 32, [](int idx, int y, int rowH, bool sel) {
    listSprite.fillRect(0, y, LIST_VIEW_W, rowH - 2, sel ? currentTheme.primary : TFT_BLACK);
    listSprite.setTextColor(sel ? TFT_BLACK : TFT_WHITE);
    listSprite.setTextSize(2);
    listSprite.setCursor(5, y + 6);
    listSprite.println(savedSignalGroups[idx]);
  });
  activeList.onOpen = []() {
    if (activeList.selectedIndex >= 0 && activeList.selectedIndex < savedSignalGroupCount) {
      currentSavedGroup = savedSignalGroups[activeList.selectedIndex];
      listGroupedSignals();
    }
  };
  createTouchBox(60, LIST_BUTTON_Y, 120, 28, currentTheme.secondary, currentTheme.secondary, "Back", signalOptions, true);
  drawTitle("Transmit > Saved", 70);
}

void startSignalListen() {
  buttonCount = 0;
  listeningForSignal = true;
  signalCaptured = false;
  currentRawDataLen = 0;
  currentDecodedHex = 0;
  clearScreen();
  printCentered("Listening", 120, currentTheme.primary, 2);
  printCentered("for signal...", 140, currentTheme.primary, 2);
  drawBackBtn(85, 200, 70, 40, []() {
    listeningForSignal = false;
    signalCaptured = false;
    signalOptions();
  });
  drawTitle("Receive > Listen", 70);
}

// ============================================================
// Screens — Built-in signals
// ============================================================
void builtInSignalsBrowser() {
  buttonCount = 0;
  builtInBrandCount = hardcodedBrandsLength;
  activeList.selectedIndex = 0;
  activeList.scrollPx = 0;
  clearScreen();

  if (builtInBrandCount == 0) {
    printCentered("No brands", 120, currentTheme.primary, 2);
    printCentered("found!", 140, currentTheme.primary, 2);
    drawBackBtn(60, 200, 120, 40, drawMenuUI);
    drawTitle("Built-in signals", 70);
    return;
  }
  setupAndRenderScrollList(builtInBrandCount, 32, [](int idx, int y, int rowH, bool sel) {
    listSprite.fillRect(0, y, LIST_VIEW_W, rowH - 2, sel ? currentTheme.primary : TFT_BLACK);
    listSprite.setTextColor(sel ? TFT_BLACK : TFT_WHITE);
    listSprite.setTextSize(2);
    listSprite.setCursor(5, y + 6);
    listSprite.println(hardcodedBrands[idx].brandName);
  });
  activeList.onOpen = []() {
    int idx = activeList.selectedIndex;
    if (idx < 0 || idx >= builtInBrandCount) return;
    currentBrandCodes = hardcodedBrands[idx].codes;
    currentBrandCodesLength = hardcodedBrands[idx].codesLength;
    currentBrandPath = hardcodedBrands[idx].brandName;
    listBuiltInSignals();
  };
  createTouchBox(60, LIST_BUTTON_Y, 120, 28, currentTheme.secondary, currentTheme.secondary, "Back", drawMenuUI, true);
  drawTitle("Built-in signals", 70);
}

void listBuiltInSignals() {
  buttonCount = 0;
  activeList.selectedIndex = 0;
  activeList.scrollPx = 0;
  clearScreen();

  if (!currentBrandCodes || currentBrandCodesLength == 0) {
    printCentered("Brand not", 120, currentTheme.primary, 2);
    printCentered("found!", 140, currentTheme.primary, 2);
    drawBackBtn(60, 200, 120, 40, builtInSignalsBrowser);
    drawTitle("Brand signals", 75);
    return;
  }
  builtInSignalCount = currentBrandCodesLength;
  setupAndRenderScrollList(builtInSignalCount, 32, [](int idx, int y, int rowH, bool sel) {
    listSprite.fillRect(0, y, LIST_VIEW_W, rowH - 2, sel ? currentTheme.primary : TFT_BLACK);
    listSprite.setTextColor(sel ? TFT_BLACK : TFT_WHITE);
    listSprite.setTextSize(2);
    listSprite.setCursor(5, y + 6);
    listSprite.println(currentBrandCodes[idx].codeName);
  });
  createTouchBox(
    15, LIST_BUTTON_Y, 100, 28, currentTheme.secondary, currentTheme.secondary, "Back",
    []() {
      builtInSignalsBrowser();
    },
    true);
  createTouchBox(125, LIST_BUTTON_Y, 100, 28, currentTheme.primary, currentTheme.primary, "Send", []() {
    if (activeList.selectedIndex < 0 || activeList.selectedIndex >= builtInSignalCount) return;
    const IRCode *code = &currentBrandCodes[activeList.selectedIndex];
    IRSignal signal;
    strncpy(signal.name, code->codeName, sizeof(signal.name) - 1);
    signal.name[sizeof(signal.name) - 1] = '\0';
    signal.rawDataLen = (uint8_t)prontoToRawSignal(code->codeArray, signal.rawData, 200);
    transmitSignal(signal);
  });
  drawTitle((currentBrandPath + " signals").c_str(), 70);
}

// ============================================================
// Screens — SD Card
// ============================================================
void sdData() {
  createOptions(SD_CARD_OPTIONS, 4, 10, 47, 220, 45);
  drawTitle("SD Card options", 75);
}

void listSDInfo() {
  buttonCount = 0;
  clearScreen();
  drawTitle("SD Card > Info", 75);
  uint64_t total = SD.totalBytes(), used = SD.usedBytes();
  int y = 110;

  tft.setTextSize(3);
  tft.setTextColor(currentTheme.primary);
  tft.setCursor(5, 72);
  tft.println("Storage");
  tft.drawFastHLine(0, y - 12, 240, currentTheme.primary);
  tft.drawFastHLine(0, y - 10, 240, currentTheme.primary);

  tft.setTextSize(2);
  auto row = [&](const char *label, String val) {
    tft.setCursor(10, y);
    tft.print(label);
    tft.println(val);
    y += 20;
  };
  row("Full: ", formatBytes(total));
  row("Free: ", formatBytes(total - used));
  row("Used: ", formatBytes(used));
  y += 20;

  tft.setTextSize(3);
  tft.setCursor(5, y);
  tft.println("Signals");
  tft.drawFastHLine(0, y + 26, 240, currentTheme.primary);
  tft.drawFastHLine(0, y + 28, 240, currentTheme.primary);
  y += 40;
  tft.setTextSize(2);
  tft.setCursor(10, y);
  tft.print("Saved: ");
  tft.println(countFilesInDirectory("/saved-signals"));

  drawBackBtn(185, 130, 55, 50, sdData);
}

void listSDFiles() {
  buttonCount = 0;
  currentPath = "/";
  activeList.selectedIndex = 0;
  activeList.scrollPx = 0;
  loadSDFiles(currentPath);
  drawSDFileBrowser();
}

void loadSDFiles(String path) {
  sdFileCount = 0;
  File dir = SD.open(path);
  if (!dir) {
    Serial.println("Failed to open dir");
    return;
  }
  for (File e = dir.openNextFile(); e && sdFileCount < 50; e = dir.openNextFile()) {
    String name = String(e.name());
    if (name.startsWith("/")) name = name.substring(1);
    sdFiles[sdFileCount++] = e.isDirectory() ? ("DIR - " + name) : (name + " - " + formatBytes(e.size()));
    e.close();
  }
  dir.close();
}

void drawSDFileBrowser() {
  buttonCount = 0;
  clearScreen();
  tft.setTextSize(1);
  tft.setTextColor(currentTheme.primary);
  tft.setCursor(5, 14);
  tft.print("Path: ");
  tft.println(currentPath);

  if (sdFileCount == 0) {
    printCentered("No files", 150, currentTheme.primary, 2);
    printCentered("found!", 170, currentTheme.primary, 2);
    drawBackBtn(60, 200, 120, 40, sdData);
    drawTitle("SD Card > Files", 75);
    return;
  }
  setupAndRenderScrollList(sdFileCount, 26, [](int idx, int y, int rowH, bool sel) {
    listSprite.fillRect(0, y, LIST_VIEW_W, rowH - 2, sel ? currentTheme.primary : TFT_BLACK);
    listSprite.setTextColor(sel ? TFT_BLACK : TFT_WHITE);
    listSprite.setTextSize(1);
    listSprite.setCursor(5, y + 6);
    String name = sdFiles[idx];
    if (name.length() > 35) name = name.substring(0, 32) + "...";
    listSprite.println(name);
  });
  activeList.onOpen = []() {
    if (activeList.selectedIndex < 0 || activeList.selectedIndex >= sdFileCount) return;
    String sel = sdFiles[activeList.selectedIndex];
    if (!sel.startsWith("DIR - ")) return;
    String dir = sel.substring(6);
    currentPath = (currentPath == "/") ? ("/" + dir) : (currentPath + "/" + dir);
    loadSDFiles(currentPath);
    activeList.selectedIndex = 0;
    activeList.scrollPx = 0;
    drawSDFileBrowser();
  };

  bool isDir = (activeList.selectedIndex >= 0 && activeList.selectedIndex < sdFileCount) && sdFiles[activeList.selectedIndex].startsWith("DIR - ");
  const char *backLabel = (currentPath == "/") ? "Back" : "Up";
  void (*backCb)() = (currentPath == "/") ? sdData : (void (*)())[]() {
    int slash = currentPath.lastIndexOf('/');
    currentPath = (slash > 0) ? currentPath.substring(0, slash) : "/";
    loadSDFiles(currentPath);
    activeList.selectedIndex = 0;
    activeList.scrollPx = 0;
    drawSDFileBrowser();
  };

  if (isDir) {
    createTouchBox(60, LIST_BUTTON_Y, 120, 28, currentTheme.secondary, currentTheme.secondary, backLabel, backCb, true);
  } else {
    createTouchBox(15, LIST_BUTTON_Y, 135, 28, currentTheme.secondary, currentTheme.secondary, backLabel, backCb, true);
    createTouchBox(160, LIST_BUTTON_Y, 65, 28, 0xF800, TFT_WHITE, "Del", deleteSelectedFile);
  }
  drawTitle("SD Card > Files", 75);
}

void sdFormatOptions() {
  buttonCount = 0;
  clearScreen();
  createOptions(SD_FORMAT_OPTIONS, 2, 10, 100);
  drawTitle("SD Card > Format", 75);
}

void formatSD() {
  File root = SD.open("/");
  if (!root) return;
  clearScreen();
  tft.setTextColor(currentTheme.primary);
  tft.setTextSize(2);
  tft.setCursor(30, 100);
  tft.println("Formatting...");

  for (File e = root.openNextFile(); e; e = root.openNextFile()) {
    String name = String(e.name());
    bool isDir = e.isDirectory();
    e.close();
    if (name == "System Volume Information" || name == "built-in-signals") {
      formatStatusLine("Skipping:", currentTheme.primary, name);
      delay(1000);
      continue;
    }
    formatStatusLine("Deleting:", currentTheme.primary, name);
    bool ok = isDir ? deleteDirectory(("/" + name).c_str()) : SD.remove(("/" + name).c_str());
    formatStatusLine(ok ? "Deleted:" : "Failed:", ok ? (uint16_t)0x07E0 : (uint16_t)0xF800, name);
    delay(1000);
  }
  root.close();
  clearScreen();
  printCentered("Formatting done!", 140, 0x07E0, 2);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(30, 170);
  tft.println("Preserved: built-in-signals");
  delay(2000);
  if (!SD.exists("/saved-signals")) SD.mkdir("/saved-signals");
  drawMenuUI();
}

void listGroupedSignals() {
  groupedSignalCount = 0;
  File dir = SD.open("/saved-signals");
  if (dir) {
    for (File e = dir.openNextFile(); e && groupedSignalCount < 50; e = dir.openNextFile()) {
      if (!e.isDirectory()) {
        String name = String(e.name());
        if (extractPrefix(name) == currentSavedGroup)
          groupedSignalFiles[groupedSignalCount++] = name;
      }
      e.close();
    }
    dir.close();
  }
  activeList.selectedIndex = 0;
  activeList.scrollPx = 0;
  drawGroupedSignalsList();
}

void drawGroupedSignalsList() {
  buttonCount = 0;
  clearScreen();
  if (groupedSignalCount == 0) {
    printCentered("No signals", 120, currentTheme.primary, 2);
    printCentered("in group!", 140, currentTheme.primary, 2);
    drawBackBtn(60, 200, 120, 40, listSavedSignals);
    drawTitle("Group signals", 75);
    return;
  }
  setupAndRenderScrollList(groupedSignalCount, 32, [](int idx, int y, int rowH, bool sel) {
    listSprite.fillRect(0, y, LIST_VIEW_W, rowH - 2, sel ? currentTheme.primary : TFT_BLACK);
    listSprite.setTextColor(sel ? TFT_BLACK : TFT_WHITE);
    listSprite.setTextSize(2);
    listSprite.setCursor(5, y + 6);
    String name = groupedSignalFiles[idx];
    name.replace(".bin", "");
    int h = name.indexOf('-');
    if (h >= 0) name = name.substring(h + 1);
    listSprite.println(name);
  });
  createTouchBox(
    15, LIST_BUTTON_Y, 100, 28, currentTheme.secondary, currentTheme.secondary, "Back",
    []() {
      listSavedSignals();
    },
    true);
  createTouchBox(125, LIST_BUTTON_Y, 100, 28, currentTheme.primary, currentTheme.primary, "Send", []() {
    if (activeList.selectedIndex < 0 || activeList.selectedIndex >= groupedSignalCount) return;
    File f = SD.open(("/saved-signals/" + groupedSignalFiles[activeList.selectedIndex]).c_str(), FILE_READ);
    if (f) {
      IRSignal signal;
      f.read((uint8_t *)&signal, sizeof(IRSignal));
      f.close();
      digitalWrite(SD_CS, HIGH);
      digitalWrite(TOUCH_CS, HIGH);
      delay(10);
      transmitSignal(signal);
    }
  });
  drawTitle((currentSavedGroup + " signals").c_str(), 70);
}

void deleteSelectedFile() {
  if (activeList.selectedIndex < 0 || activeList.selectedIndex >= sdFileCount) return;
  String sel = sdFiles[activeList.selectedIndex];
  if (sel.startsWith("DIR - ")) return;
  int dash = sel.lastIndexOf(" - ");
  String name = (dash > 0) ? sel.substring(0, dash) : sel;
  String path = (currentPath == "/") ? ("/" + name) : (currentPath + "/" + name);
  if (SD.remove(path.c_str())) {
    loadSDFiles(currentPath);
    if (activeList.selectedIndex >= sdFileCount && sdFileCount > 0)
      activeList.selectedIndex = sdFileCount - 1;
    drawSDFileBrowser();
  } else {
    printCentered("Delete", 100, 0xF800, 2);
    printCentered("failed!", 120, 0xF800, 2);
    delay(1500);
    drawSDFileBrowser();
  }
}

// ============================================================
// Screens — Theme
// ============================================================
void themeOptions() {
  createOptions(THEME_OPTIONS, 4, 10, 47, 220, 45);
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

ThemeColors themeFromIndex(uint8_t idx) {
  switch (idx) {
    case 1: return THEME_FUTURISTIC_GREEN;
    case 2: return THEME_FUTURISTIC_PURPLE;
    default: return THEME_FUTURISTIC_RED;
  }
}

void setTheme(uint8_t themeIndex) {
  prefs.begin("uniremote", false);
  prefs.putUChar("theme", themeIndex);
  prefs.end();
  ThemeColors newTheme = themeFromIndex(themeIndex);

  tft.fillScreen(TFT_BLACK);
  delay(60);
  for (int r = 0; r <= 210; r += 7) {
    tft.fillCircle(120, 160, r, newTheme.primary);
    tft.drawCircle(120, 160, r + 1, TFT_BLACK);
    tft.drawCircle(120, 160, r + 2, newTheme.accent);
    delay(10);
  }
  tft.fillRect(0, 0, 240, 320, newTheme.primary);
  delay(80);
  currentTheme = newTheme;
  drawMenuUI();
}

// ============================================================
// Keyboard
// ============================================================
void drawKeyboard() {
  buttonCount = 0;
  clearScreen();

  tft.setTextSize(1);
  tft.setTextColor(currentTheme.accent);
  tft.setCursor(5, 13);
  tft.print("Save as:");
  tft.drawRect(5, 22, 230, 18, currentTheme.primary);
  tft.setTextColor(currentTheme.primary);
  tft.setCursor(8, 26);
  tft.print(outputText);

  tft.setTextColor(currentTheme.accent);
  tft.setCursor(5, 44);
  tft.printf("Raw: %d pulses", currentRawDataLen);
  tft.setTextColor(0xF800);
  tft.setCursor(120, 44);
  tft.print("HEX:");
  tft.setTextColor(TFT_WHITE);
  if (currentDecodedHex) {
    tft.printf(" 0x%lX", (unsigned long)currentDecodedHex);
  } else {
    tft.print(" N/A");
  }
  tft.drawFastHLine(0, 55, 240, currentTheme.darkest);

  const int kW = 22, kH = 42, kG = 2, kS = kW + kG;
  const int r0y = 74, r1y = r0y + kH + 3, r2y = r1y + kH + 3;

  int x0 = (240 - (10 * kW + 9 * kG)) / 2;
  for (int i = 0; i < 10; i++)
    createTouchBox(x0 + i * kS, r0y, kW, kH, currentTheme.primary, TFT_WHITE, qwerty0[i], keyboardButtonPressed);

  x0 = (240 - (9 * kW + 8 * kG)) / 2;
  for (int i = 0; i < 9; i++)
    createTouchBox(x0 + i * kS, r1y, kW, kH, currentTheme.primary, TFT_WHITE, qwerty1[i], keyboardButtonPressed);

  x0 = (240 - (7 * kW + 6 * kG)) / 2;
  for (int i = 0; i < 7; i++)
    createTouchBox(x0 + i * kS, r2y, kW, kH, currentTheme.primary, TFT_WHITE, qwerty2[i], keyboardButtonPressed);

  const int cW = 52, cH = 40, cG = 8, ctrlY = r2y + kH + 16;
  const int cX = (240 - (4 * cW + 3 * cG)) / 2;
  createTouchBox(cX, ctrlY, cW, cH, 0xF800, TFT_WHITE, "<", keyboardButtonPressed);
  createTouchBox(cX + (cW + cG), ctrlY, cW, cH, currentTheme.dark, TFT_WHITE, "_", keyboardButtonPressed);
  createTouchBox(cX + 2 * (cW + cG), ctrlY, cW, cH, currentTheme.dark, TFT_WHITE, "-", keyboardButtonPressed);
  createTouchBox(cX + 3 * (cW + cG), ctrlY, cW, cH, 0x07E0, TFT_WHITE, ">", keyboardButtonPressed);

  createTouchBox(
    80, ctrlY + cH + 8, 80, 26, currentTheme.secondary, currentTheme.secondary, "Back",
    []() {
      outputText[0] = '\0';
      signalCaptured = false;
      buttonCount = 0;
      signalOptions();
    },
    true);

  drawTitle("Enter signal name", 55);
}

void keyboardButtonPressed() {
  const char *label = buttons[activeBtnIndex].label;

  if (strcmp(label, "<") == 0) {
    int len = strlen(outputText);
    if (len > 0) outputText[len - 1] = '\0';

  } else if (strcmp(label, ">") == 0) {
    if (strlen(outputText) == 0) return;
    IRSignal signal;
    strncpy(signal.name, outputText, MAX_SAVED_SIGNAL_CHARS);
    signal.name[MAX_SAVED_SIGNAL_CHARS] = '\0';
    memcpy(signal.rawData, currentRawData, sizeof(signal.rawData));
    signal.rawDataLen = currentRawDataLen;
    saveSignalToSD(signal);
    clearScreen();
    printCentered("Saved!", 150, currentTheme.primary, 2);
    delay(2000);
    outputText[0] = '\0';
    signalCaptured = false;
    buttonCount = 0;
    signalOptions();
    return;

  } else if (strcmp(label, "-") == 0) {
    if (strlen(outputText) > 0 && strchr(outputText, '-') == nullptr) {
      int len = strlen(outputText);
      if (len < MAX_SAVED_SIGNAL_CHARS) {
        outputText[len] = '-';
        outputText[len + 1] = '\0';
      }
    }

  } else {
    int len = strlen(outputText);
    if (len < MAX_SAVED_SIGNAL_CHARS) {
      outputText[len] = label[0];
      outputText[len + 1] = '\0';
    }
  }

  tft.fillRect(6, 23, 228, 16, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(currentTheme.primary);
  tft.setCursor(8, 26);
  tft.print(outputText);
}

// ============================================================
// IR
// ============================================================
void captureSignal() {
  if (IrReceiver.decodedIRData.rawlen == 0) {
    IrReceiver.resume();
    return;
  }
  currentRawDataLen = min((uint16_t)(IrReceiver.decodedIRData.rawlen - 1), (uint16_t)200);
  for (uint16_t i = 0; i < currentRawDataLen; i++)
    currentRawData[i] = IrReceiver.irparams.rawbuf[i + 1] * MICROS_PER_TICK;
  currentDecodedHex = IrReceiver.decodedIRData.decodedRawData;
  signalCaptured = true;
  IrReceiver.resume();
}

void saveSignalToSD(const IRSignal &signal) {
  String path = "/saved-signals/" + String(signal.name) + ".bin";
  File f = SD.open(path.c_str(), FILE_WRITE);
  if (f) {
    f.write((uint8_t *)&signal, sizeof(IRSignal));
    f.close();
  }
}

void transmitSignal(const IRSignal &signal) {
  IrSender.sendRaw(signal.rawData, signal.rawDataLen, 38);
}

uint16_t prontoToRawSignal(const uint16_t *pronto, uint16_t *outRaw, uint8_t maxOut) {
  float unit = pronto[1] * 0.241246f;
  uint16_t totalVals = (pronto[2] + pronto[3]) * 2;
  totalVals = min(totalVals, (uint16_t)min((int)maxOut, 150 - 4));
  for (uint16_t i = 0; i < totalVals; i++) outRaw[i] = (uint16_t)(pronto[4 + i] * unit);
  return totalVals;
}

// ============================================================
// SD helpers
// ============================================================
String formatBytes(uint64_t bytes) {
  if (bytes < 1024) return String(bytes) + " B";
  if (bytes < 1024 * 1024) return String(bytes / 1024.0, 1) + " KB";
  if (bytes < 1024ULL * 1024 * 1024) return String(bytes / (1024.0 * 1024.0), 1) + " MB";
  return String(bytes / (1024.0 * 1024.0 * 1024.0), 1) + " GB";
}

int countFilesInDirectory(const char *path) {
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) return 0;
  int count = 0;
  for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
    if (!e.isDirectory()) count++;
    e.close();
  }
  dir.close();
  return count;
}

bool deleteDirectory(const char *path) {
  File dir = SD.open(path);
  if (!dir) return false;
  if (!dir.isDirectory()) {
    dir.close();
    return SD.remove(path);
  }
  for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
    String ePath = String(path) + "/" + e.name();
    bool isDir = e.isDirectory();
    e.close();
    if (!(isDir ? deleteDirectory(ePath.c_str()) : SD.remove(ePath.c_str()))) {
      dir.close();
      return false;
    }
  }
  dir.close();
  return SD.rmdir(path);
}

void formatStatusLine(const char *label, uint16_t labelColor, const String &name) {
  tft.fillRect(0, 125, 240, 55, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(labelColor);
  tft.setCursor(10, 130);
  tft.print(label);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(0, 155);
  tft.println(name);
}

String extractPrefix(String filename) {
  filename.replace(".bin", "");
  int i = filename.indexOf('-');
  return (i > 0) ? filename.substring(0, i) : filename;
}

// ============================================================
// Touch system
// ============================================================
int processTouchButtons(int tx, int ty) {
  for (int i = 0; i < buttonCount; i++) {
    if (isTouchInButton(&buttons[i], tx, ty)) {
      buttons[i].pressed = true;
      drawButton(&buttons[i], true);
      activeBtnIndex = i;
      if (buttons[i].callback) buttons[i].callback();
      return i;
    }
  }
  return -1;
}

void createTouchBox(int x, int y, int w, int h, uint16_t color, uint16_t textColor, const char *label, void (*cb)(), bool isBack, bool repeatable) {
  TouchButton *btn = &buttons[buttonCount];
  btn->x = x;
  btn->y = y;
  btn->w = w;
  btn->h = h;
  btn->color = color;
  btn->textColor = textColor;
  btn->label = label;
  btn->callback = cb;
  btn->pressed = false;
  btn->isBackButton = isBack;
  btn->repeatable = repeatable;
  drawButton(btn, false);
  buttonCount++;
}

bool isTouchInButton(TouchButton *btn, int tx, int ty) {
  return tx >= btn->x && tx <= btn->x + btn->w && ty >= btn->y && ty <= btn->y + btn->h;
}

void drawButton(TouchButton *btn, bool active) {
  if (btn->isBackButton) active = !active;
  const uint16_t borderColor = active ? TFT_WHITE : btn->color;
  const int cs = 8;

  tft.startWrite();
  tft.fillRect(btn->x, btn->y, btn->w, btn->h, TFT_BLACK);
  if (active) {
    tft.fillRect(btn->x, btn->y, btn->w, btn->h, btn->color);
    tft.drawRect(btn->x, btn->y, btn->w, btn->h, TFT_WHITE);
  } else {
    tft.drawRect(btn->x - 1, btn->y - 1, btn->w + 2, btn->h + 2, currentTheme.accent);
    tft.drawRect(btn->x - 2, btn->y - 2, btn->w + 4, btn->h + 4, currentTheme.darkest);
    tft.fillRect(btn->x, btn->y, btn->w, btn->h, TFT_BLACK);
    tft.drawRect(btn->x, btn->y, btn->w, btn->h, btn->color);
    for (int j = 2; j < btn->h - 2; j += 5)
      tft.drawFastHLine(btn->x + 2, btn->y + j, btn->w - 4, currentTheme.darkest);
  }
  tft.drawFastHLine(btn->x, btn->y, cs, borderColor);
  tft.drawFastVLine(btn->x, btn->y, cs, borderColor);
  tft.drawFastHLine(btn->x + btn->w - cs, btn->y, cs, borderColor);
  tft.drawFastVLine(btn->x + btn->w - 1, btn->y, cs, borderColor);
  tft.drawFastHLine(btn->x, btn->y + btn->h - 1, cs, borderColor);
  tft.drawFastVLine(btn->x, btn->y + btn->h - cs, cs, borderColor);
  tft.drawFastHLine(btn->x + btn->w - cs, btn->y + btn->h - 1, cs, borderColor);
  tft.drawFastVLine(btn->x + btn->w - 1, btn->y + btn->h - cs, cs, borderColor);

  tft.setTextSize(2);
  int16_t tw = tft.textWidth(btn->label), th = tft.fontHeight();
  tft.setTextColor(active ? TFT_BLACK : btn->color, active ? btn->color : TFT_BLACK);
  tft.setCursor(btn->x + (btn->w - tw) / 2, btn->y + (btn->h - th) / 2);
  tft.print(btn->label);
  tft.endWrite();
}

void createOptions(const Option opts[], int count, int x, int y, int bw, int bh) {
  buttonCount = 0;
  tft.fillRect(0, 10, 240, 308, TFT_BLACK);
  for (int i = 0; i < count; i++) {
    bool isBack = (i == count - 1 && strcmp(opts[i].name, "Back") == 0);
    createTouchBox(x, y + 15 + (bh + 5) * i, bw, bh, currentTheme.primary, currentTheme.primary, opts[i].name, opts[i].callback, isBack);
  }
  drawHeaderFooter();
}