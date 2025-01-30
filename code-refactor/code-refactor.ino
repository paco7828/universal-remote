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

void setup()
{
    uniRemote.initRemote();
}

void loop()
{
}