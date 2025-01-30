#include <IRremote.hpp>
#include <Adafruit_ST7735.h>
#include <EEPROM.h>

class UniversalRemote {
private:
  int tft_cs;
  int tft_rst;
  int tft_dc;
  int ir_rx;
  int ir_tx;
  int up_btn;
  int down_btn;
  int right_btn;
  int left_btn;
  int back_btn;
  int confirm_btn;
  uint16_t currentRawData[68];
  uint8_t currentRawDataLen;

  struct IRSignal {
    char name[10];
    uint16_t rawData[68];
    uint8_t rawDataLen;
  } __attribute__((packed));

  // ********************************* Helper functions *********************************
  bool
  debounceBtn(int btn, bool &lastState) {
    bool currentState = digitalRead(btn);
    if (currentState != lastState) {
      delay(50);
      currentState = digitalRead(btn);
    }
    return currentState;
  }

  void resetVariables() {
    // listeningForSignal = false;
    // actualListen = false;
    // signalReceived = false;
    // ableToSave = false;
    // ableToWrite = false;
    // onDelScreen = false;
    // ableToScroll = false;
    // isInspecting = false;
    // cursorRow = 0;
    // cursorCol = 0;
    // outputText = "";
  }

  void drawMenuOption(int posx, int posy, int width, int height) {
    this->tft.drawRect(posx, posy, width, height, ST7735_WHITE);
  }

  // ********************************* Generalized button handler *********************************
  void handleBtn(int btn, bool &btnState, void callback()) {
    bool currentBtnState = debounceBtn(btn, btnState);
    if (currentBtnState == HIGH && btnState == LOW) {
      callback();
      btnState = HIGH;
    } else if (currentBtnState == LOW && btnState == HIGH) {
      btnState = LOW;
    }
  }

public:
  bool upBtnState;
  bool downBtnState;
  bool leftBtnState;
  bool rightBtnState;
  bool confirmBtnState;
  bool backBtnState;
  bool signalCaptured;
  int mark_excess_micros = 20;
  bool menuShown;
  Adafruit_ST7735 tft;

  // ********************************* Constructor *********************************
  UniversalRemote(int tft_cs, int tft_rst, int tft_dc, int ir_rx, int ir_tx, int up_btn, int down_btn, int right_btn, int left_btn, int back_btn, int confirm_btn)
    : tft(tft_cs, tft_dc, tft_rst) {
    this->tft_cs = tft_cs;
    this->tft_rst = tft_rst;
    this->tft_dc = tft_dc;
    this->ir_rx = ir_rx;
    this->ir_tx = ir_tx;
    this->up_btn = up_btn;
    this->down_btn = down_btn;
    this->right_btn = right_btn;
    this->left_btn = left_btn;
    this->back_btn = back_btn;
    this->confirm_btn = confirm_btn;
  }

  // ********************************* Universal remote initializing *********************************
  void initRemote() {
    this->tft.initR(INITR_BLACKTAB);
    IrSender.begin(this->ir_tx);
    IrReceiver.begin(this->ir_rx);
    pinMode(up_btn, INPUT);
    pinMode(down_btn, INPUT);
    pinMode(left_btn, INPUT);
    pinMode(right_btn, INPUT);
    pinMode(confirm_btn, INPUT);
    pinMode(back_btn, INPUT);
    menuSetup();
  }

  // ********************************* Button handling functions *********************************

  void handleUpBtn(void (*callback)()) {
    handleBtn(up_btn, upBtnState, callback);
  }

  void handleDownBtn(void (*callback)()) {
    handleBtn(down_btn, downBtnState, callback);
  }

  void handleLeftBtn(void (*callback)()) {
    handleBtn(left_btn, leftBtnState, callback);
  }

  void handleRightBtn(void (*callback)()) {
    handleBtn(right_btn, rightBtnState, callback);
  }

  void handleBackBtn(void (*callback)()) {
    handleBtn(back_btn, backBtnState, callback);
  }

  void handleConfirmBtn(void (*callback)()) {
    handleBtn(confirm_btn, confirmBtnState, callback);
  }

  // ********************************* Signal functions *********************************
  void listenForSignal() {
    if (!signalCaptured) {
      if (IrReceiver.decode()) {
        // Capture signal
        captureSignal();

        // Create signal structure
        IRSignal signal;

        // Fill signal with captured data
        strcpy(signal.name, "Signal1");  // FIX --> custom name with keyboard
        memcpy(signal.rawData, currentRawData, currentRawDataLen * sizeof(uint16_t));
        signal.rawDataLen = currentRawDataLen;

        // Save the captured signal
        this->saveSignal(signal);

        signalCaptured = true;
      }
    }
  }

  void captureSignal() {
    for (uint8_t i = 1; i < IrReceiver.decodedIRData.rawlen; i++) {
      uint32_t duration = IrReceiver.decodedIRData.rawDataPtr->rawbuf[i] * MICROS_PER_TICK;

      if (i % 2 == 1) {  // Mark
        duration -= mark_excess_micros;
      } else {  // Space
        duration += mark_excess_micros;
      }
      currentRawData[i - 1] = (uint16_t)duration;
    }
  }

  void saveSignal(const IRSignal &signal) {
    // fix --> get next available eeprom address
    int address = 0;
    EEPROM.put(address, signal);
  }

  IRSignal readSignalFromEEPROM(int index) {
    IRSignal signal;
    int address = index * sizeof(IRSignal);
    EEPROM.get(address, signal);
    return signal;
  }

  void displaySignal() {
    // Display signal on screen...
  }

  // ********************************* Screen functions *********************************
  void refreshScreen() {
    this->tft.fillScreen(ST7735_BLACK);
    this->tft.setTextColor(ST7735_WHITE);
  }

  void printText(int fontSize, uint16_t textColor, int posX, int posY, String text) {
    this->tft.setTextSize(fontSize);
    this->tft.setTextColor(textColor);
    this->tft.setCursor(posX, posY);
    this->tft.print(text);
  }

  void createBackBtn(int rectX, int rectY, int textX, int textY) {
    this->tft.setTextSize(1);
    this->tft.setTextColor(ST7735_WHITE);
    this->tft.fillRect(rectX, rectY, 30, 20, ST7735_RED);
    this->tft.setCursor(textX, textY);
    this->tft.print("BACK");
    //handleBackBtn(menuSetup); FIX CALLBACK ERROR
  }


  void createSendBtn(bool highlight) {
    this->tft.setTextColor(ST7735_WHITE);
    this->tft.fillRect(67, 117, 55, 20, highlight ? ST7735_GREEN : ST7735_BLACK);
    this->tft.setCursor(70, 120);
    this->tft.setTextColor(highlight ? ST7735_BLACK : ST7735_WHITE);
    this->tft.print("Send");
  }

  void createDelBtn(bool highlight) {
    this->tft.fillRect(7, 117, 40, 20, highlight ? ST7735_RED : ST7735_BLACK);
    this->tft.setTextColor(ST7735_WHITE);
    this->tft.setCursor(10, 120);
    this->tft.print("Del");
  }

  void highlightOption(String option) {
    this->tft.fillRect(42, 65, 40, 25, ST7735_BLACK);
    this->tft.fillRect(40, 105, 45, 25, ST7735_BLACK);
    if (option == "Yes") {
      this->tft.fillRect(40, 105, 45, 25, ST7735_RED);
    } else if (option == "No") {
      this->tft.fillRect(42, 65, 40, 25, ST7735_RED);
    }
  }

  // ********************************* EEPROM functions *********************************
  int getNextAvailableEEPROMAddress() {
    int address = 0;
    while (address < EEPROM.length()) {
      if (EEPROM.read(address) == 0xFF) {
        break;  // Found an empty spot
      }
      address += sizeof(IRSignal);
    }
    return address;
  }

  void deleteSignalFromEEPROM(int index) {
    // Clear EEPROM data at the given index
    for (int i = 0; i < sizeof(IRSignal); i++) {
      EEPROM.write(index * sizeof(IRSignal) + i, 0xFF);
    }
  }

  // ********************************* MENU OPTION functions *********************************
  void checkMemory() {
    menuShown = false;
    refreshScreen();
    printText(2, ST7735_WHITE, 30, 10, "EEPROM");

    int usedMemory = 0;
    int nonZeroAddresses = 0;

    for (int i = 0; i < EEPROM.length(); i++) {
      byte value = EEPROM.read(i);
      if (value != 0xFF) {
        usedMemory++;
        if (value != 0) {
          nonZeroAddresses++;
        }
      }
    }

    int remainingMemory = EEPROM.length() - usedMemory;
    printText(1, ST7735_WHITE, 30, 50, "Total memory: " + String(EEPROM.length()) + " b");
    printText(3, ST7735_WHITE, 70, 50, "Used memory: " + String(usedMemory) + " b");
    printText(3, ST7735_WHITE, 90, 70, "Free memory: " + String(remainingMemory) + " b");
    createBackBtn(94, 137, 98, 143);
  }

  void menuSetup() {
    resetVariables();
    menuShown = true;
    refreshScreen();
    printText(2, ST7735_WHITE, 15, 10, "Universal");
    printText(2, ST7735_WHITE, 30, 30, "Remote");
    drawMenuOption(30, 60, 75, 30);
    printText(1, ST7735_WHITE, 37, 65, "Listen for");
    printText(1, ST7735_WHITE, 48, 75, "signal");
    drawMenuOption(30, 128, 75, 30);
    printText(1, ST7735_WHITE, 53, 133, "Saved");
    printText(1, ST7735_WHITE, 48, 144, "signals");
    drawMenuOption(0, 94, 63, 30);
    printText(1, ST7735_WHITE, 15, 100, "Check");
    printText(1, ST7735_WHITE, 12, 110, "memory");
    this->tft.fillRect(65, 94, 63, 30, ST7735_RED);
    printText(1, ST7735_WHITE, 80, 100, "Delete");
    printText(1, ST7735_WHITE, 80, 110, "memory");
  }
};
