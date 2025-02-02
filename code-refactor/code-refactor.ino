#include "./UniversalRemote.h"

// Screen pins
#define TFT_CS 10
#define TFT_RST 8
#define TFT_DC 9

// IR Receiver, Transmitter pins
#define IR_RX_PIN 2
#define IR_TX_PIN 3

// Button pins
#define UP_BTN A0
#define DOWN_BTN A1
#define LEFT_BTN A2
#define RIGHT_BTN A3
#define BACK_BTN A4
#define CONFIRM_BTN A5

UniversalRemote uniRemote(TFT_CS, TFT_RST, TFT_DC, IR_RX_PIN, IR_TX_PIN, UP_BTN, DOWN_BTN, LEFT_BTN, RIGHT_BTN, BACK_BTN, CONFIRM_BTN);

void setup() {
  uniRemote.initRemote();
}

void loop() {
  if (uniRemote.menuShown) {
    // Menu state handlers
    uniRemote.handleLeftBtn(UniversalRemote::checkMemoryCallback);
    uniRemote.handleRightBtn(UniversalRemote::memoryFormatCallback);
    uniRemote.handleUpBtn(UniversalRemote::waitForSignalCallback);
    uniRemote.handleDownBtn(UniversalRemote::listMemoryDataCallback);
  } else if (uniRemote.onDelScreen) {
    // Delete screen state handlers ONLY
    uniRemote.handleUpBtn(UniversalRemote::delScreenUpCallback);
    uniRemote.handleDownBtn(UniversalRemote::delScreenDownCallback);
    uniRemote.handleConfirmBtn(UniversalRemote::deleteMemoryCallback);
    uniRemote.handleHomeBtn(UniversalRemote::menuSetupCallback);
  } else if (!uniRemote.signalCaptured && uniRemote.onListenSignal) {
    // Signal listening state
    UniversalRemote::waitForSignalCallback(&uniRemote);
  } else if (uniRemote.onKeyboard) {
    // Keyboard state handlers
    uniRemote.initKeyboardFunctionality();
    uniRemote.handleUpBtn(UniversalRemote::moveCursorUpCallback);
    uniRemote.handleDownBtn(UniversalRemote::moveCursorDownCallback);
    uniRemote.handleLeftBtn(UniversalRemote::moveCursorLeftCallback);
    uniRemote.handleRightBtn(UniversalRemote::moveCursorRightCallback);
    uniRemote.handleConfirmBtn(UniversalRemote::confirmSelectionCallback);
  } else if (uniRemote.onSavedSignals) {
    uniRemote.handleDownBtn(UniversalRemote::scrollDownCallback);
    uniRemote.handleUpBtn(UniversalRemote::scrollUpCallback);
    uniRemote.handleConfirmBtn(UniversalRemote::selectSignalCallback);
  } else if (uniRemote.onInspection) {
    if (uniRemote.inspectionChoice) {
      uniRemote.handleLeftBtn(UniversalRemote::inspectionLeftCallback);
      uniRemote.handleConfirmBtn(UniversalRemote::sendSignalCallback);
    } else {
      uniRemote.handleRightBtn(UniversalRemote::inspectionRightCallback);
      uniRemote.handleConfirmBtn(UniversalRemote::deleteSignalCallback);
    }
  }

  // Home button handler
  if (!uniRemote.menuShown) {
    uniRemote.handleHomeBtn(UniversalRemote::menuSetupCallback);
  }
}