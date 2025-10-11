#include "./UniversalRemote.h"

// Screen pins
#define TFT_CS 5
#define TFT_DC 1
#define TFT_RST 0
#define TFT_SCLK 6
#define TFT_MOSI 7

// IR Receiver, Transmitter pins
#define IR_RX_PIN 2
#define IR_TX_PIN 3

const char *brands[5] = {"EPSON", "ACER", "BENQ", "NEC", "PANASONIC"};

UniversalRemote uniRemote(TFT_CS, TFT_RST, TFT_DC, IR_RX_PIN, IR_TX_PIN, UP_BTN, DOWN_BTN, LEFT_BTN, RIGHT_BTN, BACK_BTN, CONFIRM_BTN);

void setup()
{
    uniRemote.initRemote();
}

void loop()
{
    uniRemote.handleUpBtn(UniversalRemote::epsonFreeze);    // EPSON freeze
    uniRemote.handleLeftBtn(UniversalRemote::acerFreeze);  // ACER freeze
    uniRemote.handleDownBtn(UniversalRemote::benqFreeze);  // BENQ freeze
    uniRemote.handleRightBtn(UniversalRemote::necFreeze); // NEC freeze
    uniRemote.handleHomeBtn(UniversalRemote::panasonicFreeze);  // PANASONIC freeze
}