// Importing necessary libraries
#include <IRremote.hpp>
#include <Adafruit_ST7735.h>
#include <EEPROM.h>

// Define pins for TFT screen
#define TFT_CS 10
#define TFT_RST 8
#define TFT_DC 9

// Define screen
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Define IR signal receiver and transmitter
const int IR_RECV_PIN = 2;
const int IR_SEND_PIN = 3;

// Define buttons
const int upBtn = A0;
const int downBtn = A1;
const int leftBtn = A2;
const int rightBtn = A3;
const int backBtn = A4;
const int confirmBtn = A5;

// Keyboard's characters
const char* alphabet[] = {
  "Q", "W", "E", "R", "T", "Z", "U", "I", "O", "P",
  "A", "S", "D", "F", "G", "H", "J", "K", "L",
  "Y", "X", "C", "V", "SPACE", "B", "N", "M", "<", ">"
};

// Current row of the cursor
int cursorRow = 0;

// Current column of the cursor
int cursorCol = 0;

// Keyboard's output text
String outputText = "";

// Selected default choice at "Delete memory" screen
String selectedChoice = "No";

// Previous cursor positions, used to track and refresh the screen when keyboard is present
int prevCursorRow = -1;
int prevCursorCol = -1;

// Variables for the "Saved signals" screen, used to highlight the saved options
int highlightedIndex = 0;

// Extra boolean variables (flags) to help the program catch button presses
bool menuShown;
bool listeningForSignal;
bool actualListen;
bool signalReceived;
bool ableToSave;
bool ableToWrite;
bool onDelScreen;
bool ableToScroll;
bool isInspecting;
bool inspectionChoice;

// Boolean variables for button debounce
bool upBtnState;
bool downBtnState;
bool leftBtnState;
bool rightBtnState;
bool confirmBtnState;
bool backBtnState;

// Variable used to save the captured IR signal in HEX
char nameBuffer[10];

// IR signal's structure
struct IRSignal {
  char name[10];
  uint32_t rawData;
  uint16_t address;
  uint8_t command;
  uint8_t protocol;
};

void setup() {
  // Initialize IR transmitter
  IrSender.begin(IR_SEND_PIN);

  // Initialize IR receiver
  IrReceiver.begin(IR_RECV_PIN, ENABLE_LED_FEEDBACK);

  // Initialize buttons
  pinMode(upBtn, INPUT);
  pinMode(downBtn, INPUT);
  pinMode(leftBtn, INPUT);
  pinMode(rightBtn, INPUT);
  pinMode(confirmBtn, INPUT);
  pinMode(backBtn, INPUT);

  // Initialize screen
  tft.initR(INITR_BLACKTAB);
  menuSetup();
}

void loop() {
  // Loop to listen to all direction buttons
  if (menuShown) {
    // When upBtn is pressed --> open "Listen for signal" page
    bool upBtnCurrentState = debounceBtn(upBtn, upBtnState);
    if (upBtnCurrentState == HIGH && upBtnState == LOW) {
      listenForSignal();
      upBtnState = HIGH;
    } else if (upBtnCurrentState == LOW && upBtnState == HIGH) {
      upBtnState = LOW;
    }

    // When downBtn is pressed --> open "Saved signals" page
    bool downBtnCurrentState = debounceBtn(downBtn, downBtnState);
    if (downBtnCurrentState == HIGH && downBtnState == LOW) {
      listMemoryData();
      downBtnState = HIGH;
    } else if (downBtnCurrentState == LOW && downBtnState == HIGH) {
      downBtnState = LOW;
    }

    // When leftBtn is pressed --> open "Check memory" page
    bool leftBtnCurrentState = debounceBtn(leftBtn, leftBtnState);
    if (leftBtnCurrentState == HIGH && leftBtnState == LOW) {
      leftBtnState = HIGH;
      checkMemory();
    } else if (leftBtnCurrentState == LOW && leftBtnState == HIGH) {
      leftBtnState = LOW;
    }

    // When rightBtn is pressed --> open "Delete memory" page
    bool rightBtnCurrentState = debounceBtn(rightBtn, rightBtnState);
    if (rightBtnCurrentState == HIGH && rightBtnState == LOW) {
      showDeletionScreen();
      rightBtnState = HIGH;
    } else if (rightBtnCurrentState == LOW && rightBtnState == HIGH) {
      rightBtnState = LOW;
    }
  }
  // When not on main page --> when backBtn is pressed --> navigate back to main page
  else {
    handleBackBtn();
  }
  // "Listen to signal" page's first look
  if (listeningForSignal) {
    // If confirmBtn is pressed => refresh to the "listening" page and start IrReceiver
    bool confirmBtnCurrentState = debounceBtn(confirmBtn, confirmBtnState);
    if (confirmBtnCurrentState == HIGH && confirmBtnState == LOW) {
      confirmBtnState = HIGH;
      listenForSignalInit();
      tft.setTextSize(1);
      tft.setCursor(30, 85);
      tft.print("Listening...");
      actualListen = true;
    } else if (confirmBtnCurrentState == LOW && confirmBtnState == HIGH) {
      confirmBtnState = LOW;
    }
  }

  // Listening for IR signal
  while (actualListen) {
    listeningForSignal = false;
    handleBackBtn();
    if (IrReceiver.decode()) {
      if (IrReceiver.decodedIRData.protocol != UNKNOWN) {
        // When signal is received and it's protocol is known
        signalReceived = true;
        listenForSignalInit();
        tft.setCursor(0, 65);
        tft.print("Signal: ");
        tft.print(IrReceiver.decodedIRData.decodedRawData, HEX);
        tft.setCursor(0, 85);
        tft.print("Protocol: ");
        tft.print(getProtocolString(IrReceiver.decodedIRData.protocol));
        tft.setCursor(0, 105);
        tft.print("Address: ");
        tft.print(IrReceiver.decodedIRData.address, HEX);
        tft.setCursor(0, 125);
        tft.print("Command: ");
        tft.print(IrReceiver.decodedIRData.command, HEX);
        actualListen = false;
      } else {
        listenForSignalInit();
        tft.setCursor(15, 90);
        tft.print("Unknown protocol");
        actualListen = false;
      }
      IrReceiver.resume();
    }
  }
  // When the signal is captured => ability to press confirmBtn to save
  if (signalReceived) {
    tft.setTextColor(ST7735_BLACK);
    tft.fillRect(2, 137, 30, 20, ST7735_GREEN);
    tft.setCursor(5, 143);
    tft.print("SAVE");
    ableToSave = true;
    signalReceived = false;
  }

  // When confirmBtn is pressed => keyboard will appear to give your signal a name
  if (ableToSave) {
    bool confirmBtnCurrentState = debounceBtn(confirmBtn, confirmBtnState);
    if (confirmBtnCurrentState == HIGH && confirmBtnState == LOW) {
      confirmBtnState = HIGH;
      tft.fillScreen(ST7735_BLACK);
      drawKeyboard();
      addBackBox(94, 137, 98, 143);
      tft.setCursor(5, 20);
      tft.print("Save as: ");
      ableToWrite = true;
      ableToSave = false;
    } else if (confirmBtnCurrentState == LOW && confirmBtnState == HIGH) {
      confirmBtnState = LOW;
    }
  }

  if (ableToWrite) {
    initKeyboardFunctionality();
  }

  handleScrolling();
}

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

// Function used to scroll between the options in "saved signals"
void handleScrolling() {
  while (ableToScroll) {
    // downBtn used to scroll down
    bool downBtnCurrentState = debounceBtn(downBtn, downBtnState);
    if (downBtnCurrentState == HIGH && downBtnState == LOW) {
      // FIX
      downBtnState = HIGH;
      highlightedIndex++;
      listMemoryData();  // Refresh list
    } else if (downBtnCurrentState == LOW && downBtnState == HIGH) {
      downBtnState = LOW;
    }

    // upBtn used to scroll up
    bool upBtnCurrentState = debounceBtn(upBtn, upBtnState);
    if (upBtnCurrentState == HIGH && upBtnState == LOW) {
      if (highlightedIndex > 0) {
        upBtnState = HIGH;
        highlightedIndex--;
        listMemoryData();  // Refresh list
      }
    } else if (upBtnCurrentState == LOW && upBtnState == HIGH) {
      upBtnState = LOW;
    }

    // confirmBtn used to select the option
    bool confirmBtnCurrentState = debounceBtn(confirmBtn, confirmBtnState);
    if (confirmBtnCurrentState == HIGH && confirmBtnState == LOW) {
      confirmBtnState = HIGH;
      handleConfirmButton();  // New function to handle confirm button
    } else if (confirmBtnCurrentState == LOW && confirmBtnState == HIGH) {
      confirmBtnState = LOW;
    }

    handleBackBtn();
  }
}

// Function used to handle confirmBtn on "saved signals" page
void handleConfirmButton() {
  if (ableToScroll) {
    displaySelectedSignal(highlightedIndex);
  }
}

// Function to show data about the selected signal
void displaySelectedSignal(int index) {
  isInspecting = true;
  inspectionChoice = true;
  drawInspectionScreen(index);

  while (isInspecting) {
    bool leftBtnCurrentState = debounceBtn(leftBtn, leftBtnState);
    bool rightBtnCurrentState = debounceBtn(rightBtn, rightBtnState);
    bool backBtnCurrentState = debounceBtn(backBtn, backBtnState);
    bool confirmBtnCurrentState = debounceBtn(confirmBtn, confirmBtnState);

    if (inspectionChoice) {
      // When leftBtn is pressed and "Send" is selected --> select "Del"
      if (leftBtnCurrentState == HIGH && leftBtnState == LOW) {
        leftBtnState = HIGH;
        drawInspectionScreen(index);
        drawSendBtn(false);
        drawDelBtn(true);
        inspectionChoice = false;
      } else if (leftBtnCurrentState == LOW && leftBtnState == HIGH) {
        leftBtnState = LOW;
      }
    } else {
      // When rightBtn is pressed and "Del" is selected --> select "Send"
      if (rightBtnCurrentState == HIGH && rightBtnState == LOW) {
        rightBtnState = HIGH;
        drawInspectionScreen(index);
        drawSendBtn(true);
        drawDelBtn(false);
        inspectionChoice = true;
      } else if (rightBtnCurrentState == LOW && rightBtnState == HIGH) {
        rightBtnState = LOW;
      }
    }

    // When confirmBtn is pressed when "Del" or "Send" is selected
    if (confirmBtnCurrentState == HIGH && confirmBtnState == LOW) {
      confirmBtnState = HIGH;
      IRSignal signal = readSignalAtIndex(index);
      // "Send" is highlighted
      if (inspectionChoice) {
        uint32_t rawData = signal.rawData;
        String rawDataHexString = String(rawData, HEX);
        rawDataHexString.toUpperCase();
        rawData = strtoul(rawDataHexString.c_str(), NULL, 16);
        IrSender.sendNECRaw(rawData);
        // "Del" is highlighted
      } else {
        deleteSignalFromEEPROM(index);
        // Update the memory list after deletion
        isInspecting = false;
        listMemoryData();
      }
    } else if (confirmBtnCurrentState == LOW && confirmBtnState == HIGH) {
      confirmBtnState = LOW;
    }
    // When backBtn is pressed --> go back to the memory list
    if (backBtnCurrentState == HIGH && backBtnState == LOW) {
      backBtnState = HIGH;
      isInspecting = false;
      listMemoryData();
      return;
    } else if (backBtnCurrentState == LOW && backBtnState == HIGH) {
      backBtnState = LOW;
    }
  }
}
// Function used to draw the page after selecting an option on "saved signals" page
void drawInspectionScreen(int index) {
  IRSignal signal = readSignalAtIndex(index);
  tft.fillScreen(ST7735_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(10, 5);
  tft.print("Name:");
  tft.setCursor(10, 25);
  tft.print(signal.name);
  tft.drawLine(0, 45, 127, 45, ST7735_WHITE);
  tft.setCursor(10, 50);
  tft.print("Signal:");
  tft.setCursor(10, 70);
  String rawDataString = String(signal.rawData, HEX);
  rawDataString.toUpperCase();
  tft.print(rawDataString);
  tft.drawLine(0, 90, 127, 90, ST7735_WHITE);
  drawSendBtn(inspectionChoice);
  drawDelBtn(!inspectionChoice);
}

void deleteSignalFromEEPROM(int index) {
  // Clear EEPROM data at the given index
  for (int i = 0; i < sizeof(IRSignal); i++) {
    EEPROM.write(index * sizeof(IRSignal) + i, 0xFF);
  }
}

void drawSendBtn(bool highlight) {
  tft.setTextColor(ST7735_WHITE);
  tft.fillRect(67, 117, 55, 20, highlight ? ST7735_GREEN : ST7735_BLACK);
  tft.setCursor(70, 120);
  tft.setTextColor(highlight ? ST7735_BLACK : ST7735_WHITE);
  tft.print("Send");
}

void drawDelBtn(bool highlight) {
  tft.fillRect(7, 117, 40, 20, highlight ? ST7735_RED : ST7735_BLACK);
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(10, 120);
  tft.print("Del");
}

IRSignal readSignalAtIndex(int index) {
  IRSignal signal;
  int address = index * sizeof(IRSignal);
  EEPROM.get(address, signal);
  return signal;
}

// Function to list all stored name-value pairs in EEPROM
void listMemoryData() {
  ableToScroll = true;
  menuShown = false;
  int x = 5;
  int y = 65;
  int entryCount = 0;
  int maxEntries = 4;
  int lineHeight = 20;

  tft.fillScreen(ST7735_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(30, 10);
  tft.print("Saved");
  tft.setCursor(25, 30);
  tft.print("signals");

  tft.setTextSize(1);

  int address = 0;
  int totalEntries = 0;

  while (address < EEPROM.length()) {
    IRSignal signal;
    EEPROM.get(address, signal);

    if (signal.rawData != 0xFFFFFFFF) {
      totalEntries++;
    }
    address += sizeof(IRSignal);
  }

  if (totalEntries == 0) {
    // If no valid signals are found, display a message and return early
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(x, y);
    tft.print("No valid IR signals.");
    ableToScroll = false;  // Disable scrolling since there's nothing to scroll through
    addBackBox(94, 137, 98, 143);
    return;  // Exit the function early since there's nothing to display
  }

  if (highlightedIndex >= totalEntries) {
    highlightedIndex = totalEntries - 1;
  } else if (highlightedIndex < 0) {
    highlightedIndex = 0;
  }

  address = 0;
  entryCount = 0;
  int startEntry = max(0, highlightedIndex - (maxEntries - 1));
  int displayIndex = 0;

  while (address < EEPROM.length() && displayIndex < totalEntries) {
    IRSignal signal;
    EEPROM.get(address, signal);
    if (signal.rawData != 0xFFFFFFFF) {
      if (displayIndex >= startEntry && entryCount < maxEntries) {
        String rawData = String(signal.rawData, HEX);
        rawData.toUpperCase();
        String displayText = String(signal.name) + "-" + rawData;
        if (entryCount == highlightedIndex - startEntry) {
          tft.fillRect(3, y - 7, tft.width() - 6, lineHeight, ST7735_GREEN);
          tft.setTextColor(ST7735_BLACK);
        } else {
          tft.fillRect(0, y - 7, tft.width(), lineHeight, ST7735_BLACK);
          tft.setTextColor(ST7735_WHITE);
        }
        tft.setCursor(x, y);
        tft.print(displayText);
        y += lineHeight;
        entryCount++;
      }
      displayIndex++;
    }
    address += sizeof(IRSignal);
  }

  ableToScroll = true;
  addBackBox(94, 137, 98, 143);
}

void showDeletionScreen() {
  menuShown = false;
  onDelScreen = true;
  drawDeletionScreenComps(selectedChoice);

  while (onDelScreen) {
    bool upBtnCurrentState = debounceBtn(upBtn, upBtnState);
    bool downBtnCurrentState = debounceBtn(downBtn, downBtnState);
    bool confirmBtnCurrentState = debounceBtn(confirmBtn, confirmBtnState);
    if (downBtnCurrentState == HIGH && downBtnState == LOW) {
      downBtnState = HIGH;
      if (selectedChoice == "No") {
        selectedChoice = "Yes";
        drawDeletionScreenComps(selectedChoice);
      }
    } else if (downBtnCurrentState == LOW) {
      downBtnState = LOW;
    }
    if (upBtnCurrentState == HIGH && upBtnState == LOW) {
      upBtnState = HIGH;
      if (selectedChoice == "Yes") {
        selectedChoice = "No";
        drawDeletionScreenComps(selectedChoice);
      }
    } else if (upBtnCurrentState == LOW) {
      upBtnState = LOW;
    }
    if (confirmBtnCurrentState == HIGH && confirmBtnState == LOW) {
      confirmBtnState = HIGH;
      if (selectedChoice == "Yes") {
        deleteMemory();
      } else if (selectedChoice == "No") {
        menuSetup();
      }
    } else if (confirmBtnCurrentState == LOW) {
      confirmBtnState = LOW;
    }
  }
}

void drawDeletionScreenComps(String option) {
  tft.fillScreen(ST7735_BLACK);
  tft.setTextSize(2);
  tft.setCursor(15, 15);
  tft.print("Confirm");
  tft.setCursor(10, 35);
  tft.print("deletion?");
  tft.drawRect(42, 65, 40, 25, ST7735_WHITE);   // "No" option
  tft.drawRect(40, 105, 45, 25, ST7735_WHITE);  // "Yes" option
  highlightOption(option);
  tft.setCursor(50, 70);
  tft.print("No");
  tft.setCursor(45, 110);
  tft.print("Yes");
}

void highlightOption(String option) {
  tft.fillRect(42, 65, 40, 25, ST7735_BLACK);
  tft.fillRect(40, 105, 45, 25, ST7735_BLACK);
  if (option == "Yes") {
    tft.fillRect(40, 105, 45, 25, ST7735_RED);
  } else if (option == "No") {
    tft.fillRect(42, 65, 40, 25, ST7735_RED);
  }
}

void handleBackBtn() {
  bool backBtnCurrentState = debounceBtn(backBtn, backBtnState);
  if (backBtnCurrentState == HIGH && backBtnState == LOW) {
    menuSetup();
  } else if (backBtnCurrentState == LOW && backBtnState == HIGH) {
    backBtnState = LOW;
  }
}

void resetVariables() {
  listeningForSignal = false;
  actualListen = false;
  signalReceived = false;
  ableToSave = false;
  ableToWrite = false;
  onDelScreen = false;
  ableToScroll = false;
  isInspecting = false;
  cursorRow = 0;
  cursorCol = 0;
  outputText = "";
}

void menuSetup() {
  resetVariables();
  menuShown = true;
  tft.fillScreen(ST7735_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(15, 10);
  tft.print("Universal");
  tft.setCursor(30, 30);
  tft.print("Remote");
  tft.setTextSize(1);
  tft.drawRect(30, 60, 75, 30, ST7735_WHITE);
  tft.setCursor(37, 65);
  tft.print("Listen for");
  tft.setCursor(48, 75);
  tft.print("signal");
  tft.drawRect(30, 128, 75, 30, ST7735_WHITE);
  tft.setCursor(53, 133);
  tft.print("Saved");
  tft.setCursor(48, 144);
  tft.print("signals");
  tft.drawRect(0, 94, 63, 30, ST7735_WHITE);
  tft.setCursor(15, 100);
  tft.print("Check");
  tft.setCursor(12, 110);
  tft.print("memory");
  tft.fillRect(65, 94, 63, 30, ST7735_RED);
  tft.setCursor(80, 100);
  tft.print("Delete");
  tft.setCursor(80, 110);
  tft.print("memory");
}

void checkMemory() {
  menuShown = false;
  tft.fillScreen(ST7735_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(30, 10);
  tft.print("EEPROM");

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

  tft.setTextSize(1);
  tft.setCursor(3, 50);
  tft.print("Total memory: ");
  tft.print(EEPROM.length());
  tft.print(" b");
  tft.setCursor(3, 70);
  tft.print("Used memory: ");
  tft.print(usedMemory);
  tft.print(" b");
  tft.setCursor(3, 90);
  tft.print("Free memory: ");
  tft.print(remainingMemory);
  tft.print(" b");
  addBackBox(94, 137, 98, 143);
}

void listenForSignalInit() {
  tft.fillScreen(ST7735_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(5, 10);
  tft.print("Listen for");
  tft.setCursor(30, 30);
  tft.print("signal");
  addBackBox(94, 137, 98, 143);
}

void listenForSignal() {
  listeningForSignal = true;
  menuShown = false;
  listenForSignalInit();
  tft.fillRect(32, 75, 65, 35, ST7735_GREEN);
  tft.setCursor(35, 85);
  tft.setTextColor(ST7735_BLACK);
  tft.setTextSize(2);
  tft.print("START");
}

void addBackBox(int rectX, int rectY, int textX, int textY) {
  tft.setTextSize(1);
  tft.setTextColor(ST7735_WHITE);
  tft.fillRect(rectX, rectY, 30, 20, ST7735_RED);
  tft.setCursor(textX, textY);
  tft.print("BACK");
  handleBackBtn();
}

bool debounceBtn(int btn, bool& lastState) {
  bool currentState = digitalRead(btn);
  if (currentState != lastState) {
    delay(50);
    currentState = digitalRead(btn);
  }
  return currentState;
}

void initKeyboardFunctionality() {
  if (digitalRead(upBtn) == HIGH) {
    moveCursor(-1, 0);
    delay(100);
  }
  if (digitalRead(downBtn) == HIGH) {
    moveCursor(1, 0);
    delay(100);
  }
  if (digitalRead(leftBtn) == HIGH) {
    moveCursor(0, -1);
    delay(100);
  }
  if (digitalRead(rightBtn) == HIGH) {
    moveCursor(0, 1);
    delay(100);
  }

  // Redraw keyboard only if cursor position has changed
  if (cursorRow != prevCursorRow || cursorCol != prevCursorCol) {
    drawKeyboard();
    prevCursorRow = cursorRow;
    prevCursorCol = cursorCol;
  }

  static bool buttonPressed = false;

  if (digitalRead(confirmBtn) == HIGH && !buttonPressed) {
    buttonPressed = true;
    confirmSelection();
    delay(300);
  } else if (digitalRead(confirmBtn) == LOW) {
    buttonPressed = false;
  }
}

void drawKeyboard() {
  tft.setTextSize(1);
  tft.setTextColor(ST7735_WHITE);

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

    tft.setCursor(textX, textY);
    tft.print(currentChar);
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
      tft.fillScreen(ST7735_BLACK);
      tft.setCursor(5, 20);
      tft.setTextColor(ST7735_WHITE);
      tft.print("Save as:");
      drawKeyboard();
      tft.setCursor(55, 20);
      tft.print(outputText);
      addBackBox(94, 137, 98, 143);
    }
  } else if (selectedChar == "SPACE") {
    if (outputText.length() > 0 && !outputText.endsWith(" ")) {
      outputText += " ";
    }
  } else if (selectedChar == ">" && ableToWrite) {
    // Do not add ">" to outputText
    if (outputText.length() > 0 && outputText != ">" && outputText != ">>") {
      // Only save if outputText is not just ">" or ">>"
      handleBackBtn();
      bool confirmBtnCurrentState = debounceBtn(confirmBtn, confirmBtnState);
      confirmBtnState = HIGH;
      outputText.toCharArray(nameBuffer, 10);
      IRSignal signal;
      strcpy(signal.name, nameBuffer);
      signal.rawData = IrReceiver.decodedIRData.decodedRawData;
      saveIRSignal(signal);
      tft.fillScreen(ST7735_BLACK);
      tft.setTextSize(2);
      tft.setTextColor(ST7735_WHITE);
      tft.setCursor(30, 60);
      tft.print("Saved!");
      delay(2000);
      menuSetup();
    } else {
      tft.fillScreen(ST7735_BLACK);
      tft.setTextSize(2);
      tft.setTextColor(ST7735_WHITE);
      tft.setCursor(25, 60);
      tft.print("Invalid");
      tft.setCursor(30, 80);
      tft.print("input");
      delay(2000);
      tft.fillScreen(ST7735_BLACK);
      tft.setTextSize(1);
      tft.setCursor(5, 20);
      tft.print("Save as:");
      drawKeyboard();
      tft.setCursor(55, 20);
      tft.print(outputText);
      addBackBox(94, 137, 98, 143);
    }
  } else {
    outputText += selectedChar;

    // Check if the length of outputText has reached or exceeded 11 characters
    if (outputText.length() > 10) {
      // Remove the 11th character
      outputText.remove(10);
      tft.fillScreen(ST7735_BLACK);
      tft.setTextSize(2);
      tft.setTextColor(ST7735_WHITE);
      tft.setCursor(5, 15);
      tft.print("Max length");
      tft.setCursor(10, 35);
      tft.print("10 chars!");
      tft.setTextSize(1);
      tft.setCursor(15, 55);
      tft.print("(space included)");
      delay(2500);

      // Clear the screen and resume the keyboard functionality
      tft.fillScreen(ST7735_BLACK);
      drawKeyboard();
      tft.setCursor(5, 20);
      tft.print("Save as:");
      tft.setCursor(55, 20);
      tft.print(outputText);
    } else {
      // Print the updated outputText
      tft.setCursor(55, 20);
      tft.print(outputText);
    }
  }
}

void deleteMemory() {
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(5, 60);
  tft.setTextSize(2);
  tft.print("Deleting..");
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0xFF);
  }
  tft.fillScreen(ST7735_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(30, 60);
  tft.print("Memory");
  tft.setCursor(20, 80);
  tft.print("deleted!");
  delay(2000);
  menuSetup();
}

void saveIRSignal(const IRSignal& signal) {
  int address = getNextAvailableEEPROMAddress();
  if (address + sizeof(IRSignal) <= EEPROM.length()) {
    EEPROM.put(address, signal);
  } else {
    tft.setTextSize(2);
    tft.setCursor(10, 50);
    tft.print("EEPROM full");
  }
}