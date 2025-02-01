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
  static const char *const alphabet[29];

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
    menuShown = false;
    onListenSignal = false;
    onDelScreen = false;
    signalCaptured = false;
    cursorRow = 0;
    cursorCol = 0;
    selectedChoice = "No";
  }

  void drawMenuOption(int posx, int posy, int width, int height) {
    this->tft.drawRect(posx, posy, width, height, ST7735_WHITE);
  }

  // ********************************* Generalized button handler *********************************
  void handleBtn(int btn, bool &btnState, void (*callback)(UniversalRemote *)) {
    bool currentBtnState = debounceBtn(btn, btnState);
    if (currentBtnState == HIGH && btnState == LOW) {
      callback(this);  // Pass 'this' to callback
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
  bool HomeBtnState;
  bool signalCaptured = false;
  int mark_excess_micros = 20;
  bool menuShown;
  bool onDelScreen;
  String selectedChoice = "No";
  Adafruit_ST7735 tft;
  int cursorRow = 0;
  int cursorCol = 0;
  bool onListenSignal = false;
  bool printedListening = false;

  // ********************************* Constructor *********************************
  UniversalRemote(int tft_cs, int tft_rst, int tft_dc, int ir_rx, int ir_tx, int up_btn, int down_btn, int left_btn, int right_btn, int back_btn, int confirm_btn)
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

  void handleUpBtn(void (*callback)(UniversalRemote *)) {
    handleBtn(up_btn, upBtnState, callback);
  }

  void handleDownBtn(void (*callback)(UniversalRemote *)) {
    handleBtn(down_btn, downBtnState, callback);
  }

  void handleLeftBtn(void (*callback)(UniversalRemote *)) {
    handleBtn(left_btn, leftBtnState, callback);
  }

  void handleRightBtn(void (*callback)(UniversalRemote *)) {
    handleBtn(right_btn, rightBtnState, callback);
  }

  void handleHomeBtn(void (*callback)(UniversalRemote *)) {
    handleBtn(back_btn, HomeBtnState, callback);
  }

  void handleConfirmBtn(void (*callback)(UniversalRemote *)) {
    handleBtn(confirm_btn, confirmBtnState, callback);
  }

  // ********************************* Signal functions *********************************
  void waitForSignal() {
    refreshScreen();
    if (!printedListening) {
      printText(1, ST7735_WHITE, 30, 85, "Listening...");
      printedListening = true;
    }
    if (!signalCaptured) {
      if (IrReceiver.decode()) {
        // Capture signal
        captureSignal();
        refreshScreen();
        printText(1, ST7735_WHITE, 30, 85, "Captured!");
        delay(2000);
        createHomeBtn();
        // Create signal structure
        IRSignal signal;
        drawKeyboard();
        /*
        1. Create keyboard
        2. Add typing functionality
        3. Wait for ok btn to be pressed
        4. Save signal with custom name
        5. return to main screen
        */
        /*
        // Fill signal with captured data
        strcpy(signal.name, "Signal1");  // FIX --> custom name with keyboard
        memcpy(signal.rawData, currentRawData, currentRawDataLen * sizeof(uint16_t));
        signal.rawDataLen = currentRawDataLen;

        // Save the captured signal
        this->saveSignal(signal);
        */

        signalCaptured = true;
      }
    }
  }

  void captureSignal() {
    currentRawDataLen = IrReceiver.decodedIRData.rawlen - 1;

    // Store the raw timing data
    for (uint8_t i = 1; i < IrReceiver.decodedIRData.rawlen; i++) {
      uint32_t duration = IrReceiver.decodedIRData.rawDataPtr->rawbuf[i] * MICROS_PER_TICK;

      if (i & 1) {  // Mark
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

  void createHomeBtn() {
    this->tft.setTextSize(1);
    this->tft.setTextColor(ST7735_WHITE);
    this->tft.fillRect(94, 137, 30, 20, ST7735_RED);
    this->tft.setCursor(98, 143);
    this->tft.print("HOME");
    handleHomeBtn(menuSetupCallback);
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
    printText(1, ST7735_WHITE, 3, 50, "Total memory: " + String(EEPROM.length()) + " b");
    printText(1, ST7735_WHITE, 3, 70, "Used memory: " + String(usedMemory) + " b");
    printText(1, ST7735_WHITE, 3, 90, "Free memory: " + String(remainingMemory) + " b");
    createHomeBtn();
  }

  void memoryFormat() {
    resetVariables();
    onDelScreen = true;
    selectedChoice = "No";
    drawDelScreenComps(selectedChoice);
  }

  void drawDelScreenComps(String option) {
    refreshScreen();
    printText(2, ST7735_WHITE, 15, 15, "Confirm");
    printText(2, ST7735_WHITE, 10, 35, "deletion?");
    tft.drawRect(42, 65, 40, 25, ST7735_WHITE);   // "No" option
    tft.drawRect(40, 105, 45, 25, ST7735_WHITE);  // "Yes" option
    highlightOption(option);
    printText(2, ST7735_WHITE, 50, 70, "No");
    printText(2, ST7735_WHITE, 45, 110, "Yes");
    createHomeBtn();
  }

  void deleteMemory() {
    refreshScreen();
    printText(2, ST7735_WHITE, 5, 60, "Deleting..");
    for (int i = 0; i < EEPROM.length(); i++) {
      EEPROM.write(i, 0xFF);
    }
    refreshScreen();
    printText(2, ST7735_WHITE, 30, 60, "Memory");
    printText(2, ST7735_WHITE, 20, 80, "deleted!");
    delay(2000);
    menuSetup();
  }

  // ********************************* KEYBOARD functions *********************************
  void drawKeyboard() {
    refreshScreen();

    const int cellWidth = 15;
    const int cellHeight = 24;
    const int spaceWidth = 2 * cellWidth;
    const int startX = 5;
    const int startY = 40;

    int row, col;
    for (int i = 0; i < 29; i++) {
      String currentChar = alphabet[i];
      int currentCellWidth = cellWidth;
      int xOffset, yOffset;

      if (currentChar == "N") {
        row = 3;
        col = 1;
      } else if (currentChar == "SPACE") {
        row = 3;
        col = 2;
        currentCellWidth = spaceWidth;
      } else if (currentChar == "<") {
        row = 3;
        col = 4;
      } else if (currentChar == "M") {
        row = 3;
        col = 5;
      } else if (currentChar == ">") {
        row = 3;
        col = 6;
      } else if (currentChar == "B") {
        row = 2;
        col = 7;
      } else {
        row = i / 8;
        col = i % 8;
        if (row == 3) {
          if (col >= 1 && col <= 2) col += 1;
          else if (col >= 4) col += 2;
        }
      }

      xOffset = startX + col * cellWidth;
      yOffset = startY + row * cellHeight;

      if (row == cursorRow && (col == cursorCol || (currentChar == "SPACE" && (cursorCol == 2 || cursorCol == 3)))) {
        if (currentChar == ">") {
          tft.fillRect(xOffset, yOffset, currentCellWidth, cellHeight, ST7735_GREEN);
          tft.setTextColor(ST7735_BLACK);
        } else if (currentChar == "<") {
          tft.fillRect(xOffset, yOffset, currentCellWidth, cellHeight, ST7735_RED);
          tft.setTextColor(ST7735_WHITE);
        } else {
          tft.fillRect(xOffset, yOffset, currentCellWidth, cellHeight, ST7735_BLUE);
          tft.setTextColor(ST7735_WHITE);
        }
      } else {
        if (currentChar == ">") {
          tft.fillRect(xOffset + 1, yOffset + 1, currentCellWidth - 2, cellHeight - 2, ST7735_BLACK);
          tft.setTextColor(ST7735_GREEN);
        } else if (currentChar == "<") {
          tft.fillRect(xOffset + 1, yOffset + 1, currentCellWidth - 2, cellHeight - 2, ST7735_BLACK);
          tft.setTextColor(ST7735_RED);
        } else {
          tft.fillRect(xOffset + 1, yOffset + 1, currentCellWidth - 2, cellHeight - 2, ST7735_BLACK);
          tft.setTextColor(ST7735_WHITE);
        }
        tft.drawRect(xOffset, yOffset, currentCellWidth, cellHeight, ST7735_WHITE);
      }

      int textX, textY, textWidth, textHeight;
      tft.getTextBounds(currentChar, 0, 0, &textX, &textY, &textWidth, &textHeight);
      textX = xOffset + (currentCellWidth - textWidth) / 2;
      textY = yOffset + (cellHeight - textHeight) / 2;

      if (currentChar == "SPACE") textX += 1;
      printText(1, ST7735_WHITE, textX, textY, currentChar);
    }
  }



  // ********************************* CALLBACK functions *********************************

  static void menuSetupCallback(UniversalRemote *remote) {
    remote->menuSetup();
  }

  static void checkMemoryCallback(UniversalRemote *remote) {
    remote->checkMemory();
  }

  static void memoryFormatCallback(UniversalRemote *remote) {
    remote->memoryFormat();
  }

  static void drawDelScreenCompsCallback(UniversalRemote *remote) {
    remote->drawDelScreenComps(remote->selectedChoice);
  }

  static void deleteMemoryCallback(UniversalRemote *remote) {
    if (remote->selectedChoice == "Yes") {
      remote->deleteMemory();
    } else if (remote->selectedChoice == "No") {
      remote->menuSetup();
    }
  }

  static void waitForSignalCallback(UniversalRemote *remote) {
    remote->waitForSignal();
  }

  static void delScreenDownCallback(UniversalRemote *remote) {
    if (remote->onDelScreen && remote->selectedChoice == "No") {
      remote->selectedChoice = "Yes";
      remote->drawDelScreenComps(remote->selectedChoice);
    }
  }

  static void delScreenUpCallback(UniversalRemote *remote) {
    if (remote->onDelScreen && remote->selectedChoice == "Yes") {
      remote->selectedChoice = "No";
      remote->drawDelScreenComps(remote->selectedChoice);
    }
  }
};

// Created outside of the class
const char *const UniversalRemote::alphabet[29] = {
  "Q", "W", "E", "R", "T", "Z", "U", "I", "O", "P",
  "A", "S", "D", "F", "G", "H", "J", "K", "L",
  "Y", "X", "C", "V", "SPACE", "B", "N", "M", "<", ">"
};
