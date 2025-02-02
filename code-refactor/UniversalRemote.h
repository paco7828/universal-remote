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
    String name;
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
    onDelScreen = false;
    onListenSignal = false;
    printedListening = false;
    signalCaptured = false;
    onKeyboard = false;
    cursorRow = 0;
    cursorCol = 0;
    selectedChoice = "No";
    outputText = "";
    currentRawData[68] = {};
    currentRawDataLen = 0;
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
  bool onKeyboard = false;
  int prevCursorRow = -1;
  int prevCursorCol = -1;
  String outputText = "";

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
    if (!onListenSignal) {
      menuShown = false;
      onListenSignal = true;
      if (!printedListening) {
        refreshScreen();
        printText(1, ST7735_WHITE, 30, 85, "Listening...");
        printedListening = true;
      }
    }
    if (!signalCaptured) {
      if (IrReceiver.decode()) {
        // Capture signal
        captureSignal();
        refreshScreen();
        printText(1, ST7735_WHITE, 30, 85, "Captured!");
        delay(2000);
        createHomeBtn();
        drawKeyboard();
        signalCaptured = true;
      }
      IrReceiver.resume();
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
    int address = getNextAvailableEEPROMAddress();
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
    onKeyboard = true;
    refreshScreen();
    printText(1, ST7735_WHITE, 5, 20, "Save as:");
    createHomeBtn();

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
    prevCursorRow = cursorRow;
    prevCursorCol = cursorCol;
  }

  void initKeyboardFunctionality() {
    // Redraw keyboard only if cursor position has changed
    if (cursorRow != prevCursorRow || cursorCol != prevCursorCol) {
      drawKeyboard();
      printText(1, ST7735_WHITE, 55, 20, outputText);
    }
  }

  void moveCursor(int rowChange, int colChange) {
    int newRow = cursorRow + rowChange;
    int newCol = cursorCol + colChange;

    if (newRow < 0) {
      newRow = 0;
    } else if (newRow > 3) {
      newRow = 3;
    }

    if (newRow == 3) {
      if (newCol < 0) {
        newCol = 0;
      } else if (newCol > 7) {
        newCol = 7;
      }

      if (cursorRow == 2 && cursorCol == 7 && rowChange == 1) {
        return;  // Do nothing if trying to move down from "B"
      }

      if (cursorRow == 3 && cursorCol == 1 && colChange == -1) {
        return;  // Do nothing if trying to move left from "N"
      }

      if (cursorRow == 2 && cursorCol == 0 && rowChange == 1) {
        return;  // Do nothing if trying to move down from "J"
      }

      if (cursorRow == 3 && cursorCol == 6 && (colChange == 1 || rowChange == 1)) {
        return;  // Do nothing if trying to move right or down from ">"
      }

      if (cursorCol == 2 && colChange == 1) {
        // Currently on first SPACE cell, move to the next key after SPACE
        newCol = 4;
      } else if (cursorCol == 3 && colChange == -1) {
        newCol = 1;
      } else if (cursorCol == 3 && colChange == -1) {
        // Currently on second SPACE cell, move to the first SPACE cell
        newCol = 2;
      } else if (cursorCol == 4 && colChange == -1) {
        // Currently on key after SPACE, move to second SPACE cell
        newCol = 3;
      }
    } else {
      // Handle rows 0 to 2
      if (newCol < 0) {
        newCol = 0;  // Stay within the first column
      } else if (newCol > 7) {
        newCol = 7;  // Stay within the last column
      }
    }

    // Update cursor position
    cursorRow = newRow;
    cursorCol = newCol;
  }

  void confirmSelection() {
    // Calculate the index of the selected character
    int charIndex = cursorRow * 8 + cursorCol;

    // Adjust the index to account for special keys in the bottom row
    if (cursorRow == 2 && cursorCol == 7) {
      charIndex = 24;  // Index for "B"
    } else if (cursorRow == 3) {
      // Handle the bottom row explicitly for special keys
      if (cursorCol == 1) charIndex = 25;                         // "N"
      else if (cursorCol == 2 || cursorCol == 3) charIndex = 23;  // "SPACE"
      else if (cursorCol == 4) charIndex = 27;                    // "<"
      else if (cursorCol == 5) charIndex = 26;                    // "M"
      else if (cursorCol == 6) charIndex = 28;                    // ">"
    } else {
      // Adjust for rows 0 to 2
      if (cursorCol >= 0 && cursorCol <= 7) {
        charIndex = cursorRow * 8 + cursorCol;
      }
    }

    // Ensure charIndex is within bounds
    if (charIndex < 0 || charIndex >= sizeof(alphabet) / sizeof(alphabet[0])) {
      return;  // Index out of bounds, do nothing
    }

    String selectedChar = alphabet[charIndex];

    // Handle character addition, space, and deletion
    if (selectedChar == "<") {
      if (outputText.length() > 0 && outputText != "") {
        outputText.remove(outputText.length() - 1);
        refreshScreen();
        drawKeyboard();
        printText(1, ST7735_WHITE, 55, 20, outputText);
      }
    } else if (selectedChar == "SPACE") {
      if (outputText.length() > 0 && !outputText.endsWith(" ")) {
        outputText += " ";
      }
    } else if (selectedChar == ">") {
      // Do not add ">" to outputText
      if (outputText.length() > 0 && outputText != ">" && outputText != ">>") {
        // Only save if outputText is not just ">" or ">>"
        // SAVING LOGIC GOES HERE
        //
        //
        //
        refreshScreen();
        // Fill signal with captured data
        IRSignal signal;
        signal.name = outputText;
        memcpy(signal.rawData, currentRawData, currentRawDataLen * sizeof(uint16_t));
        signal.rawDataLen = currentRawDataLen;

        // Save the captured signal
        saveSignal(signal);
        printText(2, ST7735_WHITE, 30, 60, "Saved!");
        delay(2000);
        menuSetup();
      } else {
        refreshScreen();
        printText(2, ST7735_WHITE, 25, 60, "Invalid");
        printText(2, ST7735_WHITE, 30, 80, "input");
        delay(2000);
        refreshScreen();
        drawKeyboard();
        printText(1, ST7735_WHITE, 55, 20, outputText);
      }
    } else {
      outputText += selectedChar;

      // Check if the length of outputText has reached or exceeded 11 characters
      if (outputText.length() > 10) {
        // Remove the 11th character
        outputText.remove(10);
        refreshScreen();
        printText(2, ST7735_WHITE, 5, 15, "Max length");
        printText(2, ST7735_WHITE, 10, 35, "10 chars!");
        printText(1, ST7735_WHITE, 15, 55, "(space included)");
        delay(2500);

        // Clear the screen and resume the keyboard functionality
        refreshScreen();
        drawKeyboard();
        printText(1, ST7735_WHITE, 55, 20, outputText);
      }
    }
    printText(1, ST7735_WHITE, 55, 20, outputText);
  }

  // ********************************* CALLBACK functions *********************************

  static void
  menuSetupCallback(UniversalRemote *remote) {
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

  static void moveCursorUpCallback(UniversalRemote *remote) {
    remote->moveCursor(-1, 0);
  }

  static void moveCursorDownCallback(UniversalRemote *remote) {
    remote->moveCursor(1, 0);
  }

  static void moveCursorLeftCallback(UniversalRemote *remote) {
    remote->moveCursor(0, -1);
  }

  static void moveCursorRightCallback(UniversalRemote *remote) {
    remote->moveCursor(0, 1);
  }

  static void confirmSelectionCallback(UniversalRemote *remote) {
    remote->confirmSelection();
  }
};

// Created outside of the class
const char *const UniversalRemote::alphabet[29] = {
  "Q", "W", "E", "R", "T", "Z", "U", "I", "O", "P",
  "A", "S", "D", "F", "G", "H", "J", "K", "L",
  "Y", "X", "C", "V", "SPACE", "B", "N", "M", "<", ">"
};
