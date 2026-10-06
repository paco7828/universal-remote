#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <SPI.h>
#include <SD.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <Preferences.h>
#include <functional>
#include <vector>
#include <algorithm>
#include "./IR-codes.h"

// ============================================================
// Layout of this file
//   1. Pins, display driver
//   2. Constants, types, theme presets
//   3. Function prototypes (grouped by module)
//   4. Static data (menu tables, built-in brand table)
//   5. Global state (grouped by module)
//   6. Setup & loop
//   7. UI core: display chrome, touch & buttons, scroll list engine
//   8. IR engine, signal storage (SD), SD utilities
//   9. Screens: main menu, signals, built-in, SD card, theme
// ============================================================

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
// Constants
// ============================================================
// --- UI layout: scroll list viewport shared by every list screen ---
constexpr int LIST_VIEW_X = 5;
constexpr int LIST_VIEW_Y = 26;
constexpr int LIST_VIEW_W = 230;
constexpr int LIST_VIEW_H = 228;
constexpr int LIST_BUTTON_Y = 260;

// --- Touch timing ---
constexpr unsigned long REPEAT_INTERVAL = 200;
constexpr int SCROLL_DRAG_THRESHOLD = 10;
constexpr unsigned long DOUBLE_TAP_WINDOW = 400;

// --- IR capture / transmit ---
constexpr uint16_t IR_CAPTURE_BUFFER = 1024;
constexpr uint8_t IR_CAPTURE_TIMEOUT_MS = 35;
constexpr uint16_t IR_FRAME_WINDOW_MS = 400;
constexpr uint8_t MAX_FRAMES = 3;
constexpr uint16_t MAX_RAW_PULSES = 400;
constexpr uint8_t DEFAULT_CARRIER_KHZ = 38;
constexpr uint16_t IR_DEFAULT_GAP_MS = 40;   // raw keretek közti szünet, ha nincs mentett
constexpr uint16_t IR_VARIANT_GAP_MS = 50;   // built-in kódvariánsok közti szünet
constexpr float PRONTO_CLOCK_US = 0.241246f;

// --- Signal storage on SD ---
constexpr const char *SAVED_ROOT = "/saved-signals";
constexpr const char *SIGNAL_EXT = ".txt";
constexpr int MAX_SAVED_SIGNAL_CHARS = 25;
constexpr int SD_FILES_MAX = 500;  // SD file browser cap

// ============================================================
// Types
// ============================================================
enum SignalKind : uint8_t { SIG_RAW = 0,
                            SIG_VALUE = 1,
                            SIG_STATE = 2 };
enum CaptureResult : uint8_t { CAP_IGNORED,
                               CAP_INVALID,
                               CAP_OK };

struct IRSignal {
  char name[MAX_SAVED_SIGNAL_CHARS + 1];
  SignalKind kind;
  decode_type_t protocol;
  uint16_t bits;  // SIG_VALUE: bitszám, SIG_STATE: bájtszám*8
  uint64_t value;
  uint8_t state[kStateSizeMax];
  uint8_t carrierKHz;
  uint16_t rawData[MAX_RAW_PULSES];
  uint16_t rawDataLen;
  uint16_t gapAfterMs;
};

struct TouchButton {
  int x, y, w, h;
  uint16_t color;
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
// Theme presets
// ============================================================
const ThemeColors THEME_FUTURISTIC_RED = { 0xF800, 0xD000, 0x8800, 0x4208, 0x2104 };
const ThemeColors THEME_FUTURISTIC_GREEN = { 0x07E0, 0x05C0, 0x0400, 0x4208, 0x2104 };
const ThemeColors THEME_FUTURISTIC_PURPLE = { 0xF81F, 0xC81F, 0x8010, 0x4208, 0x2104 };

// ============================================================
// Function prototypes
// ============================================================
// Setup & loop
void initHardware();
void pollIRCapture();
void pollTouch();

// Display chrome
void clearScreen();
void beginScreen();
void drawHeaderFooter();
void drawTitle(const char *title, uint16_t x = 90, uint16_t y = 305);
void drawBackBtn(uint8_t x, uint8_t y, uint8_t w, uint8_t h, void (*cb)());
void printCentered(const char *text, int y, uint16_t color, uint8_t size);
void showEmptyScreen(const char *line1, const char *line2, int y, void (*backCb)(), const char *title, uint16_t titleX);
void drawBootSplash();

// Touch & buttons
int processTouchButtons(int tx, int ty);
bool isTouchInButton(TouchButton *btn, int tx, int ty);
void drawButton(TouchButton *btn, bool active);
void createTouchBox(int x, int y, int w, int h, uint16_t color, const char *label, void (*cb)(), bool isBack = false, bool repeatable = false);
void createOptions(const Option opts[], int count, int x = 10, int y = 50, int bw = 220, int bh = 45);

// Scroll list engine
bool pointInScrollView(ScrollList *list, int x, int y);
void ensureListSprite(int w, int h);
void clampScroll(ScrollList &list);
void renderScrollList(ScrollList &list);
void setupAndRenderScrollList(int count, int rowH, RowRenderer renderer);
void resetListSelection();
bool selectionValid(size_t count);
void drawListRow(int y, int rowH, bool selected, const String &text, uint8_t textSize = 2);

// IR engine
CaptureResult captureFrame(const decode_results &r, unsigned long nowMs);
void transmitSignal(const IRSignal &signal);
void transmitSequence(const IRSignal *frames, uint8_t count);
uint8_t prontoCarrierKHz(const uint16_t *pronto);
uint16_t prontoToRawSignal(const uint16_t *pronto, uint16_t *outRaw);
void sendBuiltInCode(const IRCode &code);

// Signal storage (SD)
void sortNames(std::vector<String> &v);
void splitSignalName(const char *full, String &group, String &file);
void migrateFlatSignals();
bool isSignalFile(const String &fileName);
String stripSignalExt(String fileName);
void saveSignalToSD(const IRSignal *frames, uint8_t count);
bool loadSignalFromSD(const String &fileName, IRSignal *frames, uint8_t &count);

// SD utilities
String formatBytes(uint64_t bytes);
int countFilesInDirectory(const char *path);
bool deleteDirectory(const char *path);
void formatStatusLine(const char *label, uint16_t labelColor, const String &name);

// Screens: main menu
void drawMenuUI();

// Screens: signals (transmit / receive / keyboard)
void signalOptions();
void listSavedSignals();
void drawSavedSignalsList();
void listGroupedSignals();
void drawGroupedSignalsList();
void startSignalListen();
void drawKeyboard();
void keyboardButtonPressed();
void leaveKeyboard();

// Screens: built-in signals
void builtInSignalsBrowser();
void listBuiltInSignals();

// Screens: SD card
void sdData();
void listSDInfo();
void listSDFiles();
void browseSDPath(const String &path);
void loadSDFiles(const String &path);
void drawSDFileBrowser();
void sdFormatOptions();
void formatSD();
void deleteSelectedFile();

// Screens: theme
void themeOptions();
void setThemeFuturisticRed();
void setThemeFuturisticGreen();
void setThemeFuturisticPurple();
ThemeColors themeFromIndex(uint8_t idx);
void setTheme(uint8_t themeIndex);

// ============================================================
// Static data (menus & brand library)
// ============================================================
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
  { "NEC", NEC_CODES, NEC_CODES_LENGTH }
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

// --- Button system ---
TouchButton buttons[30];
uint8_t buttonCount = 0;
uint8_t activeBtnIndex = 0;

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

// --- Scroll list engine ---
LGFX_Sprite listSprite(&tft);
bool listSpriteReady = false;
ScrollList activeList;
ScrollList *activeScrollList = nullptr;

// --- IR ---
IRrecv irrecv(IR_RX, IR_CAPTURE_BUFFER, IR_CAPTURE_TIMEOUT_MS, true);
IRsend irsend(IR_TX);
decode_results irResults;
IRSignal capturedFrames[MAX_FRAMES];
uint8_t capturedFrameCount = 0;
unsigned long lastFrameMs = 0;
bool signalCaptured = false;
bool listeningForSignal = false;
bool rcToggle = false;  // RC5/RC6 toggle bit, váltva minden új gombnyomásnál

// --- Saved signals browser ---
std::vector<String> savedSignalGroups;
std::vector<String> groupedSignalFiles;
String currentSavedGroup = "";
String sendCachePath;  // utoljára betöltött jel (hold-to-repeat-hez)

// --- Built-in signals browser ---
const IRCode *currentBrandCodes = nullptr;
uint8_t currentBrandCodesLength = 0;
String currentBrandName = "";

// --- SD file browser ---
std::vector<String> sdFiles;
String currentPath = "/";

// --- Keyboard ---
const char *const qwerty0[10] = { "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P" };
const char *const qwerty1[9] = { "A", "S", "D", "F", "G", "H", "J", "K", "L" };
const char *const qwerty2[7] = { "Z", "X", "C", "V", "B", "N", "M" };
char outputText[MAX_SAVED_SIGNAL_CHARS + 1] = "";

// ============================================================
// Setup & loop
// ============================================================
void setup() {
  initHardware();
}

void loop() {
  if (listeningForSignal && !signalCaptured) pollIRCapture();
  pollTouch();
  delay(listeningForSignal ? 1 : 10);
}

void pollIRCapture() {
  if (irrecv.decode(&irResults)) {
    unsigned long now = millis();
    CaptureResult cr = captureFrame(irResults, now);
    Serial.printf("frame @%lu %s %ub %s\n", now, typeToString(irResults.decode_type).c_str(),
                  irResults.bits, cr == CAP_OK ? "stored" : "skipped");
    if (cr == CAP_INVALID) {
      beginScreen();
      printCentered("Invalid", 120, currentTheme.primary, 2);
      printCentered("signal!", 140, currentTheme.primary, 2);
      delay(2000);
      startSignalListen();
    }
  }

  bool done = capturedFrameCount > 0 && (capturedFrames[0].kind == SIG_RAW || capturedFrameCount >= MAX_FRAMES || millis() - lastFrameMs > IR_FRAME_WINDOW_MS);
  if (done) {
    irrecv.disableIRIn();
    signalCaptured = true;
    listeningForSignal = false;
    for (uint8_t i = 0; i < capturedFrameCount; i++) {
      const IRSignal &f = capturedFrames[i];
      Serial.printf("  #%u kind=%u %s bits=%u gap=%ums", i + 1, f.kind,
                    typeToString(f.protocol).c_str(), f.bits, f.gapAfterMs);
      if (f.kind == SIG_STATE) {
        Serial.print(" state:");
        for (uint16_t b = 0; b < f.bits / 8; b++) Serial.printf(" %02X", f.state[b]);
      }
      Serial.println();
    }
    beginScreen();
    printCentered("Captured!", 150, currentTheme.primary, 2);
    delay(1500);
    drawKeyboard();
  }
}

void pollTouch() {
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
}

void initHardware() {
  Serial.begin(115200);
  prefs.begin("uniremote", true);
  currentTheme = themeFromIndex(prefs.getUChar("theme", 0));
  prefs.end();

  irsend.begin();

  for (uint8_t cs : { TFT_CS, TOUCH_CS, SD_CS }) {
    pinMode(cs, OUTPUT);
    digitalWrite(cs, HIGH);
  }

  spiSD.begin(TFT_CLK, TFT_MISO, TFT_MOSI, SD_CS);
  initializedSD = SD.begin(SD_CS, spiSD, 20000000);
  if (initializedSD) {
    if (!SD.exists(SAVED_ROOT)) SD.mkdir(SAVED_ROOT);
    migrateFlatSignals();
  }

  tft.init();
  tft.setRotation(0);
  drawBootSplash();
}

// ============================================================
// Display chrome
// ============================================================
void clearScreen() {
  tft.fillRect(0, 10, 240, 308, TFT_BLACK);
  drawHeaderFooter();
}

void drawHeaderFooter() {
  activeScrollList = nullptr;
  activeList.onOpen = nullptr;
  lastTapIndex = -1;
  tft.drawFastHLine(0, 6, 239, currentTheme.primary);
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
  createTouchBox(x, y, w, h, currentTheme.secondary, "Back", cb, true);
}

void drawBootSplash() {
  tft.fillScreen(TFT_BLACK);
  drawHeaderFooter();
  printCentered("UNIVERSAL", 120, currentTheme.primary, 3);
  printCentered("REMOTE", 155, currentTheme.primary, 3);
  delay(2000);
  drawMenuUI();
}

// Fresh screen: drops all buttons, clears the body, redraws header/footer
void beginScreen() {
  buttonCount = 0;
  clearScreen();
}

// Shared "empty list" screen: two centered lines, Back button, title
void showEmptyScreen(const char *line1, const char *line2, int y, void (*backCb)(), const char *title, uint16_t titleX) {
  printCentered(line1, y, currentTheme.primary, 2);
  printCentered(line2, y + 20, currentTheme.primary, 2);
  drawBackBtn(60, 200, 120, 40, backCb);
  drawTitle(title, titleX);
}

// ============================================================
// Touch & buttons
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

void createTouchBox(int x, int y, int w, int h, uint16_t color, const char *label, void (*cb)(), bool isBack, bool repeatable) {
  TouchButton *btn = &buttons[buttonCount];
  btn->x = x;
  btn->y = y;
  btn->w = w;
  btn->h = h;
  btn->color = color;
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
  beginScreen();
  for (int i = 0; i < count; i++) {
    bool isBack = (i == count - 1 && strcmp(opts[i].name, "Back") == 0);
    createTouchBox(x, y + 15 + (bh + 5) * i, bw, bh, currentTheme.primary, opts[i].name, opts[i].callback, isBack);
  }
}

// ============================================================
// Scroll list engine
// ============================================================
bool pointInScrollView(ScrollList *list, int x, int y) {
  return list
         && x >= list->viewX && x <= list->viewX + list->viewW
         && y >= list->viewY && y <= list->viewY + list->viewH;
}

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

void resetListSelection() {
  activeList.selectedIndex = 0;
  activeList.scrollPx = 0;
}

// True if the list's selected row exists in a collection of `count` items
bool selectionValid(size_t count) {
  return activeList.selectedIndex >= 0 && (size_t)activeList.selectedIndex < count;
}

// Standard row used by every list screen
void drawListRow(int y, int rowH, bool selected, const String &text, uint8_t textSize) {
  listSprite.fillRect(0, y, LIST_VIEW_W, rowH - 2, selected ? currentTheme.primary : TFT_BLACK);
  listSprite.setTextColor(selected ? TFT_BLACK : TFT_WHITE);
  listSprite.setTextSize(textSize);
  listSprite.setCursor(5, y + 6);
  listSprite.println(text);
}

// ============================================================
// IR engine
// ============================================================
CaptureResult captureFrame(const decode_results &r, unsigned long nowMs) {
  if (r.repeat) return CAP_IGNORED;
  if (capturedFrameCount >= MAX_FRAMES) return CAP_IGNORED;
  bool first = (capturedFrameCount == 0);

  IRSignal &s = capturedFrames[capturedFrameCount];
  memset(&s, 0, sizeof(s));
  s.protocol = r.decode_type;
  s.carrierKHz = DEFAULT_CARRIER_KHZ;

  uint16_t n = (r.rawlen > 1) ? min((uint16_t)(r.rawlen - 1), MAX_RAW_PULSES) : 0;
  uint32_t durUs = 0;
  for (uint16_t i = 0; i < n; i++) {
    s.rawData[i] = r.rawbuf[i + 1] * kRawTick;
    durUs += s.rawData[i];
  }
  s.rawDataLen = n;

  if (r.decode_type == UNKNOWN) {
    if (!first) return CAP_IGNORED;  // ismeretlen keret csak egyedüli lehet
    if (n < 10) return CAP_INVALID;
    s.kind = SIG_RAW;
  } else if (hasACState(r.decode_type)) {
    uint16_t nbytes = min((uint16_t)(r.bits / 8), (uint16_t)kStateSizeMax);
    memcpy(s.state, r.state, nbytes);
    s.kind = SIG_STATE;
    s.bits = nbytes * 8;
  } else {
    s.kind = SIG_VALUE;
    s.value = r.value;
    s.bits = r.bits;
  }

  if (!first) {
    IRSignal &p = capturedFrames[capturedFrameCount - 1];
    // azonos keret (pl. Sony 3x ismétlés) eldobása
    if (p.kind == s.kind && p.protocol == s.protocol && p.bits == s.bits && p.value == s.value && memcmp(p.state, s.state, sizeof(s.state)) == 0)
      return CAP_IGNORED;
    // keretszünet = dekódolások közti idő - ennek a keretnek a hossza
    long gap = (long)(nowMs - lastFrameMs) - (long)(durUs / 1000);
    p.gapAfterMs = (uint16_t)constrain(gap, 10L, 300L);
  }
  capturedFrameCount++;
  lastFrameMs = nowMs;
  return CAP_OK;
}

void transmitSignal(const IRSignal &s) {
  bool ok = true;
  bool usedRaw = false;
  uint32_t t0 = micros();
  uint8_t khz = s.carrierKHz ? s.carrierKHz : DEFAULT_CARRIER_KHZ;

  if (s.kind == SIG_VALUE) {
    uint64_t v = s.value;
    if (rcToggle) {
      if (s.protocol == RC5 || s.protocol == RC5X) v = irsend.toggleRC5(v);
      else if (s.protocol == RC6) v = irsend.toggleRC6(v, s.bits);
    }
    ok = irsend.send(s.protocol, v, s.bits, IRsend::minRepeats(s.protocol));
  } else if (s.kind == SIG_STATE) {
    ok = irsend.send(s.protocol, s.state, s.bits / 8);
  } else {
    irsend.sendRaw(s.rawData, s.rawDataLen, khz);
    usedRaw = true;
  }

  if (!ok && !usedRaw && s.rawDataLen > 0) {
    irsend.sendRaw(s.rawData, s.rawDataLen, khz);
    usedRaw = true;
  }
  Serial.printf("TX kind=%u proto=%s bits=%u raw=%u ok=%d fallback=%d dur=%lums\n", s.kind,
                typeToString(s.protocol).c_str(), s.bits, s.rawDataLen, ok, !ok && usedRaw,
                (unsigned long)((micros() - t0) / 1000));
}

void transmitSequence(const IRSignal *frames, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    transmitSignal(frames[i]);
    // nyers küldésnél nincs záró szünet; state/value küldésnél a protokoll saját szünete benne van
    if (i + 1 < count && frames[i].kind == SIG_RAW)
      delay(frames[i].gapAfterMs ? frames[i].gapAfterMs : IR_DEFAULT_GAP_MS);
  }
}

uint8_t prontoCarrierKHz(const uint16_t *pronto) {
  return (uint8_t)(1000.0f / (pronto[1] * PRONTO_CLOCK_US) + 0.5f);  // 0x6D->38, 0x6B->39, 0x70->37
}

uint16_t prontoToRawSignal(const uint16_t *pronto, uint16_t *outRaw) {
  const float unit = pronto[1] * PRONTO_CLOCK_US;
  uint16_t totalVals = (pronto[2] + pronto[3]) * 2;
  totalVals = min(totalVals, (uint16_t)(IR_CODE_LEN - 4));  // 4 fejlécszó után jön az adat
  for (uint16_t i = 0; i < totalVals; i++) outRaw[i] = (uint16_t)(pronto[4 + i] * unit);
  return totalVals;
}

void sendBuiltInCode(const IRCode &code) {
  IRSignal signal{};
  signal.kind = SIG_RAW;

  for (uint8_t v = 0; v < code.variantCount && v < MAX_IR_VARIANTS; v++) {
    signal.rawDataLen = prontoToRawSignal(code.codeArray[v], signal.rawData);
    signal.carrierKHz = prontoCarrierKHz(code.codeArray[v]);
    transmitSignal(signal);
    if (v + 1 < code.variantCount) delay(IR_VARIANT_GAP_MS);
  }
}

// ============================================================
// Signal storage (SD)
// ============================================================
void sortNames(std::vector<String> &v) {
  std::sort(v.begin(), v.end(), [](const String &a, const String &b) {
    return strcasecmp(a.c_str(), b.c_str()) < 0;
  });
}

void splitSignalName(const char *full, String &group, String &file) {
  const char *d = strchr(full, '-');
  if (d && d != full && d[1]) {
    group = String(full).substring(0, d - full);
    file = String(d + 1);
  } else {
    group = "Misc";
    file = full;
  }
}

void migrateFlatSignals() {
  File dir = SD.open(SAVED_ROOT);
  if (!dir) return;
  std::vector<String> flat;
  for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
    if (!e.isDirectory() && isSignalFile(String(e.name()))) flat.push_back(String(e.name()));
    e.close();
  }
  dir.close();
  for (auto &n : flat) {
    String stem = stripSignalExt(n), group, file;
    splitSignalName(stem.c_str(), group, file);
    String d = String(SAVED_ROOT) + "/" + group;
    if (!SD.exists(d.c_str())) SD.mkdir(d.c_str());
    SD.rename((String(SAVED_ROOT) + "/" + n).c_str(), (d + "/" + file + SIGNAL_EXT).c_str());
  }
}

bool isSignalFile(const String &fileName) {
  String n = fileName;
  n.toLowerCase();
  return n.endsWith(SIGNAL_EXT);
}

String stripSignalExt(String fileName) {
  if (isSignalFile(fileName)) fileName.remove(fileName.length() - strlen(SIGNAL_EXT));
  return fileName;
}

void saveSignalToSD(const IRSignal *frames, uint8_t count) {
  String group, file;
  splitSignalName(frames[0].name, group, file);
  String dir = String(SAVED_ROOT) + "/" + group;
  if (!SD.exists(dir.c_str())) SD.mkdir(dir.c_str());
  String path = dir + "/" + file + SIGNAL_EXT;
  File f = SD.open(path.c_str(), FILE_WRITE);
  if (!f) return;
  f.println("# UniRemote IR signal");
  f.print("name: ");
  f.println(frames[0].name);
  f.print("carrier_khz: ");
  f.println(frames[0].carrierKHz);

  for (uint8_t n = 0; n < count; n++) {
    const IRSignal &s = frames[n];
    f.print("frame: ");
    f.println(n + 1);
    if (n + 1 < count) {
      f.print("gap_ms: ");
      f.println(s.gapAfterMs);
    }
    if (s.kind == SIG_VALUE) {
      f.print("protocol: ");
      f.println(typeToString(s.protocol));
      f.print("bits: ");
      f.println(s.bits);
      f.print("value: 0x");
      f.println(uint64ToString(s.value, 16));
    } else if (s.kind == SIG_STATE) {
      f.print("protocol: ");
      f.println(typeToString(s.protocol));
      f.print("bytes: ");
      for (uint16_t i = 0; i < s.bits / 8; i++) {
        if (i) f.print(' ');
        if (s.state[i] < 0x10) f.print('0');
        f.print(s.state[i], HEX);
      }
      f.println();
    }
    if (s.rawDataLen > 0) {
      f.print("length: ");
      f.println(s.rawDataLen);
      f.print("raw: ");
      for (uint16_t i = 0; i < s.rawDataLen; i++) {
        if (i) f.print(',');
        f.print(s.rawData[i]);
      }
      f.println();
    }
  }
  f.close();
}

bool loadSignalFromSD(const String &fileName, IRSignal *frames, uint8_t &count) {
  count = 0;
  File f = SD.open((String(SAVED_ROOT) + "/" + fileName).c_str(), FILE_READ);
  if (!f) return false;

  memset(frames, 0, sizeof(IRSignal) * MAX_FRAMES);
  uint8_t carrier = DEFAULT_CARRIER_KHZ, cur = 0, seen = 1;  // "frame:" nélküli (régi) fájl = 1 keret
  bool hasValue[MAX_FRAMES] = {}, hasRaw[MAX_FRAMES] = {};
  uint16_t stateLen[MAX_FRAMES] = {};

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line[0] == '#') continue;
    int colon = line.indexOf(':');
    if (colon <= 0) continue;
    String key = line.substring(0, colon);
    key.trim();
    key.toLowerCase();
    String val = line.substring(colon + 1);
    val.trim();

    if (key == "carrier_khz") {
      int k = val.toInt();
      if (k >= 30 && k <= 60) carrier = (uint8_t)k;
      continue;
    }
    if (key == "frame") {
      int n = val.toInt();
      if (n >= 1 && n <= MAX_FRAMES) {
        cur = n - 1;
        if (cur + 1 > seen) seen = cur + 1;
      } else {
        cur = 0xFF;
      }
      continue;
    }
    if (cur >= MAX_FRAMES) continue;
    IRSignal &s = frames[cur];

    if (key == "gap_ms") {
      s.gapAfterMs = (uint16_t)val.toInt();
    } else if (key == "protocol") {
      s.protocol = strToDecodeType(val.c_str());
    } else if (key == "bits") {
      s.bits = (uint16_t)val.toInt();
    } else if (key == "value") {
      s.value = strtoull(val.c_str(), nullptr, 16);
      hasValue[cur] = true;
    } else if (key == "bytes") {
      const char *p = val.c_str();
      char *end;
      stateLen[cur] = 0;
      while (*p && stateLen[cur] < kStateSizeMax) {
        unsigned long v = strtoul(p, &end, 16);
        if (end == p) {
          p++;
          continue;
        }
        s.state[stateLen[cur]++] = (uint8_t)v;
        p = end;
      }
    } else if (key == "raw") {
      const char *p = val.c_str();
      char *end;
      s.rawDataLen = 0;
      while (*p && s.rawDataLen < MAX_RAW_PULSES) {
        unsigned long v = strtoul(p, &end, 10);
        if (end == p) {
          p++;
          continue;
        }
        s.rawData[s.rawDataLen++] = (uint16_t)min(v, 65535UL);
        p = end;
      }
      hasRaw[cur] = s.rawDataLen > 0;
    }
  }
  f.close();

  for (uint8_t n = 0; n < seen; n++) {
    IRSignal &s = frames[n];
    s.carrierKHz = carrier;
    if (hasValue[n] && s.protocol != UNKNOWN && s.bits) s.kind = SIG_VALUE;
    else if (stateLen[n] && s.protocol != UNKNOWN) {
      s.kind = SIG_STATE;
      s.bits = stateLen[n] * 8;
    } else if (hasRaw[n]) s.kind = SIG_RAW;
    else break;
    count = n + 1;
  }

  return count > 0;
}

// ============================================================
// SD utilities
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
    if (e.isDirectory()) {
      String sub = String(path) + "/" + e.name();
      e.close();
      count += countFilesInDirectory(sub.c_str());
    } else {
      count++;
      e.close();
    }
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

// ============================================================
// Screens — Main menu
// ============================================================
void drawMenuUI() {
  tft.fillScreen(TFT_BLACK);
  if (initializedSD) {
    createOptions(MENU_OPTIONS, 4, 10, 27, 220, 52);
  } else {
    createOptions(MENU_OPTIONS_NO_SD, 3, 10, 40, 220, 62);
  }
  drawTitle("MENU", 110);
}

// ============================================================
// Screens — Signals (transmit / receive / keyboard)
// ============================================================
void signalOptions() {
  beginScreen();
  const int btnSize = 100, gap = 10;
  const int startX = (240 - btnSize * 2 - gap) / 2;
  createTouchBox(startX, 70, btnSize, btnSize, currentTheme.primary, "Transmit", listSavedSignals);
  createTouchBox(startX + btnSize + gap, 70, btnSize, btnSize, currentTheme.primary, "Receive", startSignalListen);
  createTouchBox(60, 190, 120, 45, currentTheme.secondary, "Back", drawMenuUI, true);
  drawTitle("Signal options", 80);
}

void listSavedSignals() {
  savedSignalGroups.clear();
  File dir = SD.open(SAVED_ROOT);
  if (dir) {
    for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
      if (e.isDirectory()) savedSignalGroups.push_back(String(e.name()));
      e.close();
    }
    dir.close();
  }
  sortNames(savedSignalGroups);
  resetListSelection();
  drawSavedSignalsList();
}

void drawSavedSignalsList() {
  beginScreen();
  if (savedSignalGroups.empty()) {
    showEmptyScreen("No signals", "saved!", 150, signalOptions, "Transmit > Saved", 70);
    return;
  }
  setupAndRenderScrollList((int)savedSignalGroups.size(), 32, [](int idx, int y, int rowH, bool sel) {
    drawListRow(y, rowH, sel, savedSignalGroups[idx]);
  });
  activeList.onOpen = []() {
    if (!selectionValid(savedSignalGroups.size())) return;
    currentSavedGroup = savedSignalGroups[activeList.selectedIndex];
    listGroupedSignals();
  };
  createTouchBox(60, LIST_BUTTON_Y, 120, 28, currentTheme.secondary, "Back", signalOptions, true);
  drawTitle("Transmit > Saved", 70);
}

void listGroupedSignals() {
  sendCachePath = "";
  groupedSignalFiles.clear();
  String path = String(SAVED_ROOT) + "/" + currentSavedGroup;
  File dir = SD.open(path.c_str());
  if (dir) {
    for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
      if (!e.isDirectory() && isSignalFile(String(e.name())))
        groupedSignalFiles.push_back(String(e.name()));
      e.close();
    }
    dir.close();
  }
  sortNames(groupedSignalFiles);
  resetListSelection();
  drawGroupedSignalsList();
}

void drawGroupedSignalsList() {
  beginScreen();
  if (groupedSignalFiles.empty()) {
    showEmptyScreen("No signals", "in group!", 120, listSavedSignals, "Group signals", 75);
    return;
  }
  setupAndRenderScrollList((int)groupedSignalFiles.size(), 32, [](int idx, int y, int rowH, bool sel) {
    drawListRow(y, rowH, sel, stripSignalExt(groupedSignalFiles[idx]));
  });
  createTouchBox(15, LIST_BUTTON_Y, 100, 28, currentTheme.secondary, "Back", listSavedSignals, true);
  createTouchBox(
    125, LIST_BUTTON_Y, 100, 28, currentTheme.primary, "Send", []() {
      if (!selectionValid(groupedSignalFiles.size())) return;
      static IRSignal frames[MAX_FRAMES];
      static uint8_t count = 0;
      String path = currentSavedGroup + "/" + groupedSignalFiles[activeList.selectedIndex];
      if (path != sendCachePath) {
        if (!loadSignalFromSD(path, frames, count)) {
          sendCachePath = "";
          return;
        }
        sendCachePath = path;
      }
      bool isRepeat = heldButtonIndex >= 0;
      if (isRepeat && frames[0].kind == SIG_STATE) return;
      digitalWrite(SD_CS, HIGH);
      digitalWrite(TOUCH_CS, HIGH);
      delay(10);
      if (!isRepeat) rcToggle = !rcToggle;
      transmitSequence(frames, count);
    },
    false, true);
  drawTitle((currentSavedGroup + " signals").c_str(), 70);
}

void startSignalListen() {
  listeningForSignal = true;
  signalCaptured = false;
  capturedFrameCount = 0;
  irrecv.enableIRIn();
  beginScreen();
  printCentered("Listening", 120, currentTheme.primary, 2);
  printCentered("for signal...", 140, currentTheme.primary, 2);
  drawBackBtn(85, 200, 70, 40, []() {
    irrecv.disableIRIn();
    listeningForSignal = false;
    signalCaptured = false;
    signalOptions();
  });
  drawTitle("Receive > Listen", 70);
}

void drawKeyboard() {
  beginScreen();

  tft.setTextSize(1);
  tft.setTextColor(currentTheme.accent);
  tft.setCursor(5, 13);
  tft.print("Save as:");
  tft.drawRect(5, 22, 230, 18, currentTheme.primary);
  tft.setTextColor(currentTheme.primary);
  tft.setCursor(8, 26);
  tft.print(outputText);

  const IRSignal &c0 = capturedFrames[0];
  tft.setTextColor(currentTheme.accent);
  tft.setCursor(5, 44);
  if (c0.kind == SIG_RAW) tft.printf("Raw: %d pulses", c0.rawDataLen);
  else tft.printf("%s %db x%u", typeToString(c0.protocol).c_str(), c0.bits, capturedFrameCount);
  tft.setTextColor(0xF800);
  tft.setCursor(120, 44);
  tft.print("HEX:");
  tft.setTextColor(TFT_WHITE);
  if (c0.kind == SIG_VALUE) tft.printf(" 0x%lX", (unsigned long)c0.value);
  else tft.print(" N/A");
  tft.drawFastHLine(0, 55, 240, currentTheme.darkest);

  const int kW = 22, kH = 42, kG = 2, kS = kW + kG;
  const int r0y = 74, r1y = r0y + kH + 3, r2y = r1y + kH + 3;

  int x0 = (240 - (10 * kW + 9 * kG)) / 2;
  for (int i = 0; i < 10; i++)
    createTouchBox(x0 + i * kS, r0y, kW, kH, currentTheme.primary, qwerty0[i], keyboardButtonPressed);

  x0 = (240 - (9 * kW + 8 * kG)) / 2;
  for (int i = 0; i < 9; i++)
    createTouchBox(x0 + i * kS, r1y, kW, kH, currentTheme.primary, qwerty1[i], keyboardButtonPressed);

  x0 = (240 - (7 * kW + 6 * kG)) / 2;
  for (int i = 0; i < 7; i++)
    createTouchBox(x0 + i * kS, r2y, kW, kH, currentTheme.primary, qwerty2[i], keyboardButtonPressed);

  const int cW = 52, cH = 40, cG = 8, ctrlY = r2y + kH + 16;
  const int cX = (240 - (4 * cW + 3 * cG)) / 2;
  createTouchBox(cX, ctrlY, cW, cH, 0xF800, "<", keyboardButtonPressed);
  createTouchBox(cX + (cW + cG), ctrlY, cW, cH, currentTheme.dark, "_", keyboardButtonPressed);
  createTouchBox(cX + 2 * (cW + cG), ctrlY, cW, cH, currentTheme.dark, "-", keyboardButtonPressed);
  createTouchBox(cX + 3 * (cW + cG), ctrlY, cW, cH, 0x07E0, ">", keyboardButtonPressed);

  createTouchBox(80, ctrlY + cH + 8, 80, 26, currentTheme.secondary, "Back", leaveKeyboard, true);

  drawTitle("Enter signal name", 55);
}

void keyboardButtonPressed() {
  const char *label = buttons[activeBtnIndex].label;

  if (strcmp(label, "<") == 0) {
    int len = strlen(outputText);
    if (len > 0) outputText[len - 1] = '\0';

  } else if (strcmp(label, ">") == 0) {
    if (strlen(outputText) == 0) return;
    strncpy(capturedFrames[0].name, outputText, MAX_SAVED_SIGNAL_CHARS);
    capturedFrames[0].name[MAX_SAVED_SIGNAL_CHARS] = '\0';
    saveSignalToSD(capturedFrames, capturedFrameCount);
    clearScreen();
    printCentered("Saved!", 150, currentTheme.primary, 2);
    delay(2000);
    leaveKeyboard();
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

// Leaves the name-entry screen (after save or Back) and drops the captured signal
void leaveKeyboard() {
  outputText[0] = '\0';
  signalCaptured = false;
  signalOptions();
}

// ============================================================
// Screens — Built-in signals
// ============================================================
void builtInSignalsBrowser() {
  resetListSelection();
  beginScreen();
  setupAndRenderScrollList(hardcodedBrandsLength, 32, [](int idx, int y, int rowH, bool sel) {
    drawListRow(y, rowH, sel, hardcodedBrands[idx].brandName);
  });
  activeList.onOpen = []() {
    if (!selectionValid(hardcodedBrandsLength)) return;
    const HardcodedBrand &brand = hardcodedBrands[activeList.selectedIndex];
    currentBrandCodes = brand.codes;
    currentBrandCodesLength = brand.codesLength;
    currentBrandName = brand.brandName;
    listBuiltInSignals();
  };
  createTouchBox(60, LIST_BUTTON_Y, 120, 28, currentTheme.secondary, "Back", drawMenuUI, true);
  drawTitle("Built-in signals", 70);
}

void listBuiltInSignals() {
  resetListSelection();
  beginScreen();
  setupAndRenderScrollList(currentBrandCodesLength, 32, [](int idx, int y, int rowH, bool sel) {
    drawListRow(y, rowH, sel, currentBrandCodes[idx].codeName);
  });
  createTouchBox(15, LIST_BUTTON_Y, 100, 28, currentTheme.secondary, "Back", builtInSignalsBrowser, true);
  createTouchBox(125, LIST_BUTTON_Y, 100, 28, currentTheme.primary, "Send", []() {
    if (!selectionValid(currentBrandCodesLength)) return;
    sendBuiltInCode(currentBrandCodes[activeList.selectedIndex]);
  });
  drawTitle((currentBrandName + " signals").c_str(), 70);
}

// ============================================================
// Screens — SD Card
// ============================================================
void sdData() {
  createOptions(SD_CARD_OPTIONS, 4, 10, 47, 220, 45);
  drawTitle("SD Card options", 75);
}

void listSDInfo() {
  beginScreen();
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
  tft.println(countFilesInDirectory(SAVED_ROOT));

  drawBackBtn(185, 130, 55, 50, sdData);
}

void browseSDPath(const String &path) {
  currentPath = path;
  loadSDFiles(currentPath);
  resetListSelection();
  drawSDFileBrowser();
}

void listSDFiles() {
  browseSDPath("/");
}

void loadSDFiles(const String &path) {
  sdFiles.clear();
  File dir = SD.open(path);
  if (!dir) {
    Serial.println("Failed to open dir");
    return;
  }
  for (File e = dir.openNextFile(); e && sdFiles.size() < SD_FILES_MAX; e = dir.openNextFile()) {
    String name = String(e.name());
    if (name.startsWith("/")) name = name.substring(1);
    sdFiles.push_back(e.isDirectory() ? ("DIR - " + name) : (name + " - " + formatBytes(e.size())));
    e.close();
  }
  dir.close();
}

void drawSDFileBrowser() {
  beginScreen();
  tft.setTextSize(1);
  tft.setTextColor(currentTheme.primary);
  tft.setCursor(5, 14);
  tft.print("Path: ");
  tft.println(currentPath);

  if (sdFiles.empty()) {
    showEmptyScreen("No files", "found!", 150, sdData, "SD Card > Files", 75);
    return;
  }
  setupAndRenderScrollList((int)sdFiles.size(), 26, [](int idx, int y, int rowH, bool sel) {
    String name = sdFiles[idx];
    if (name.length() > 35) name = name.substring(0, 32) + "...";
    drawListRow(y, rowH, sel, name, 1);
  });
  activeList.onOpen = []() {
    if (!selectionValid(sdFiles.size())) return;
    String sel = sdFiles[activeList.selectedIndex];
    if (!sel.startsWith("DIR - ")) return;
    String dir = sel.substring(6);
    browseSDPath((currentPath == "/") ? ("/" + dir) : (currentPath + "/" + dir));
  };

  bool isDir = selectionValid(sdFiles.size()) && sdFiles[activeList.selectedIndex].startsWith("DIR - ");
  const char *backLabel = (currentPath == "/") ? "Back" : "Up";
  void (*backCb)() = (currentPath == "/") ? sdData : (void (*)())[]() {
    int slash = currentPath.lastIndexOf('/');
    browseSDPath((slash > 0) ? currentPath.substring(0, slash) : String("/"));
  };

  if (isDir) {
    createTouchBox(60, LIST_BUTTON_Y, 120, 28, currentTheme.secondary, backLabel, backCb, true);
  } else {
    createTouchBox(15, LIST_BUTTON_Y, 135, 28, currentTheme.secondary, backLabel, backCb, true);
    createTouchBox(160, LIST_BUTTON_Y, 65, 28, 0xF800, "Del", deleteSelectedFile);
  }
  drawTitle("SD Card > Files", 75);
}

void sdFormatOptions() {
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
  if (!SD.exists(SAVED_ROOT)) SD.mkdir(SAVED_ROOT);
  drawMenuUI();
}

void deleteSelectedFile() {
  if (!selectionValid(sdFiles.size())) return;
  String sel = sdFiles[activeList.selectedIndex];
  if (sel.startsWith("DIR - ")) return;
  int dash = sel.lastIndexOf(" - ");
  String name = (dash > 0) ? sel.substring(0, dash) : sel;
  String path = (currentPath == "/") ? ("/" + name) : (currentPath + "/" + name);
  if (SD.remove(path.c_str())) {
    loadSDFiles(currentPath);
    if (!sdFiles.empty() && activeList.selectedIndex >= (int)sdFiles.size())
      activeList.selectedIndex = (int)sdFiles.size() - 1;
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
