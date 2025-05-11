#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 9

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

void setup() {
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("Scan a card...");

  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
}

bool readBlock(byte block, byte *buffer, byte *bufferSize) {
  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid)
  );
  if (status != MFRC522::STATUS_OK) return false;

  status = mfrc522.MIFARE_Read(block, buffer, bufferSize);
  return (status == MFRC522::STATUS_OK);
}

bool writeBlock(byte block, byte *data) {
  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid)
  );
  if (status != MFRC522::STATUS_OK) return false;

  status = mfrc522.MIFARE_Write(block, data, 16);
  return (status == MFRC522::STATUS_OK);
}

void waitForSerialInput(char* input, int maxLen) {
  int idx = 0;
  while (idx < maxLen - 1) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n') break;
      input[idx++] = c;
    }
  }
  input[idx] = '\0';
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;
  Serial.println("Card detected");

  byte buffer[18];
  byte size = sizeof(buffer);

  // Read block 2 (plate)
  if (readBlock(2, buffer, &size)) {
    Serial.print("Current plate: ");
    Serial.write(buffer, 16);
    Serial.println();
  } else {
    Serial.println("Failed to read plate block.");
  }

  // Read block 4 (balance)
  if (readBlock(4, buffer, &size)) {
    Serial.print("Current balance: ");
    Serial.write(buffer, 16);
    Serial.println();
  } else {
    Serial.println("Failed to read balance block.");
  }

  // Ask for new data only if plate block is empty
  if (buffer[0] == 0x00 || buffer[0] == 0xFF) {
    char newPlate[17] = {0};
    char newBalance[17] = {0};

    Serial.println("Enter new plate number (max 16 chars):");
    waitForSerialInput(newPlate, 17);

    Serial.println("Enter new balance (max 16 chars):");
    waitForSerialInput(newBalance, 17);

    byte plateData[16] = {0};
    byte balanceData[16] = {0};
    strncpy((char*)plateData, newPlate, 16);
    strncpy((char*)balanceData, newBalance, 16);

    if (writeBlock(2, plateData)) {
      Serial.println("Plate written.");
    } else {
      Serial.println("Failed to write plate.");
    }

    if (writeBlock(4, balanceData)) {
      Serial.println("Balance written.");
    } else {
      Serial.println("Failed to write balance.");
    }
  } else {
    Serial.println("Blocks not empty. Skipping write.");
  }

  // Final state
  if (readBlock(2, buffer, &size)) {
    Serial.print("Final plate: ");
    Serial.write(buffer, 16);
    Serial.println();
  }

  if (readBlock(4, buffer, &size)) {
    Serial.print("Final balance: ");
    Serial.write(buffer, 16);
    Serial.println();
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  Serial.println("\nReady for next card...");
  delay(3000);
}
