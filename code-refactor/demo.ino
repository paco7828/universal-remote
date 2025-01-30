// Import necessary libraries
#include <IRremote.hpp>
#include <EEPROM.h>

// Define IR signal receiver and transmitter pins
const int IR_RECV_PIN = 2;
const int IR_SEND_PIN = 3;

// Define IR signal structure
struct IRSignal {
  char name[10];            // Signal name
  uint16_t rawData[68];     // Fixed length
  uint8_t rawDataLen;       // Length of raw data
} __attribute__((packed));  // Prevents memory alignment issues

// Buffer for capturing IR signals
uint16_t currentRawData[68];  
uint8_t currentRawDataLen;
#define MARK_EXCESS_MICROS 20

// State variables
bool signalCaptured = false;
unsigned long lastTransmitTime = 0;
const unsigned long TRANSMIT_INTERVAL = 3000;  // 3 seconds

void setup() {
  Serial.begin(115200);
  IrSender.begin(IR_SEND_PIN);
  IrReceiver.begin(IR_RECV_PIN);  // FIXED: Removed ENABLE_LED_FEEDBACK

  Serial.println("IR Remote Ready!");
  Serial.println("Waiting for signal...");
}

void loop() {
  if (!signalCaptured) {
    if (IrReceiver.decode()) {
      Serial.println("\nSignal received!");

      // Capture the raw data
      captureRawData();

      // Create signal structure
      IRSignal signal;
      strcpy(signal.name, "Signal1");  
      memcpy(signal.rawData, currentRawData, currentRawDataLen * sizeof(uint16_t));
      signal.rawDataLen = currentRawDataLen;

      // Debug print the captured data
      Serial.println("Captured raw data:");
      printRawData(signal.rawData, signal.rawDataLen);

      // Save to EEPROM
      saveIRSignal(signal);

      signalCaptured = true;
      lastTransmitTime = millis();

      Serial.println("Signal saved to EEPROM");
      Serial.println("Starting periodic transmission...");
      
      IrReceiver.resume();  // FIXED: Immediately resume receiver
    }
  } else {
    if (millis() - lastTransmitTime >= TRANSMIT_INTERVAL) {
      Serial.println("\nReading signal from EEPROM and transmitting...");

      // Read from EEPROM
      IRSignal savedSignal = readSignalAtIndex(0);

      // Debug print the read data
      Serial.println("Data read from EEPROM:");
      printRawData(savedSignal.rawData, savedSignal.rawDataLen);

      // Send the signal
      sendIRSignal(savedSignal);
      Serial.println("Signal transmitted!");

      lastTransmitTime = millis();
    }
  }
}

// --- FIXED CAPTURE FUNCTION ---
void captureRawData() {
  currentRawDataLen = IrReceiver.decodedIRData.rawlen - 1;
  
  if (currentRawDataLen > 68) {
    Serial.println("ERROR: Raw data too long! Truncating.");
    currentRawDataLen = 68;
  }

  Serial.print("Capturing raw data, length: ");
  Serial.println(currentRawDataLen);

  for (uint8_t i = 1; i < IrReceiver.decodedIRData.rawlen; i++) {
    uint32_t duration = IrReceiver.decodedIRData.rawDataPtr->rawbuf[i] * MICROS_PER_TICK;

    if (i % 2 == 1) {  // Mark
      duration -= MARK_EXCESS_MICROS;
    } else {  // Space
      duration += MARK_EXCESS_MICROS;
    }

    if (duration > 65535) {
      Serial.println("WARNING: Overflow detected in raw data!");
      duration = 65535;
    }

    currentRawData[i - 1] = (uint16_t)duration;
  }
}

// --- FIXED SENDING FUNCTION ---
void sendIRSignal(const IRSignal &signal) {
  Serial.println("Sending IR signal:");
  printRawData(signal.rawData, signal.rawDataLen);

  IrSender.sendRaw(signal.rawData, signal.rawDataLen, 38);
  delay(50);
}

// --- EEPROM FUNCTIONS ---
void saveIRSignal(const IRSignal &signal) {
  int address = 0;  // Always overwrite first slot
  EEPROM.put(address, signal);
  Serial.println("Signal saved successfully");
}

IRSignal readSignalAtIndex(int index) {
  IRSignal signal;
  int address = index * sizeof(IRSignal);
  EEPROM.get(address, signal);
  return signal;
}

// --- DEBUG FUNCTION ---
void printRawData(uint16_t *data, uint8_t length) {
  for (uint8_t i = 0; i < length; i++) {
    Serial.print(data[i]);
    Serial.print(", ");
    if ((i + 1) % 8 == 0) {
      Serial.println();
    }
  }
  Serial.println();
}
