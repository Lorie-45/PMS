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
  Serial.println("Scan a card to read plate and balance...");

  // Default key for authentication (6 bytes of 0xFF)
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
}

// Function to read 16 bytes from a block after authentication
bool readBlock(byte block, byte *buffer, byte *bufferSize) {
  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid)
  );
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Authentication failed for block ");
    Serial.print(block);
    Serial.print(": ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  status = mfrc522.MIFARE_Read(block, buffer, bufferSize);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Read failed for block ");
    Serial.print(block);
    Serial.print(": ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }
  return true;
}

// Function to write 16 bytes to a block after authentication
bool writeBlock(byte block, byte *buffer) {
  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid)
  );
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Authentication failed for block ");
    Serial.print(block);
    Serial.print(": ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  status = mfrc522.MIFARE_Write(block, buffer, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Write failed for block ");
    Serial.print(block);
    Serial.print(": ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }
  return true;
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  Serial.println("Card detected");

  byte buffer[18];
  byte size = sizeof(buffer);

  // Read plate number from block 2
  String plate = "";
  if (readBlock(2, buffer, &size)) {
    for (byte i = 0; i < 16; i++) {
      if (buffer[i] >= 32 && buffer[i] <= 126) {
        plate += (char)buffer[i];
      }
    }
  } else {
    Serial.println("Failed to read plate number.");
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(3000);
    return;
  }

  // Read balance from block 4
  String balanceStr = "";
  size = sizeof(buffer);
  if (readBlock(4, buffer, &size)) {
    for (byte i = 0; i < 16; i++) {
      if (buffer[i] >= '0' && buffer[i] <= '9') {
        balanceStr += (char)buffer[i];
      }
    }
  } else {
    Serial.println("Failed to read balance.");
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(3000);
    return;
  }

  int balance = balanceStr.toInt();

  // Send plate and current balance to PC
  Serial.print("PLATE:");
  Serial.println(plate);
  Serial.print("CURRENT_BALANCE:");
  Serial.println(balance);

  // Wait for due amount from PC
  unsigned long startWait = millis();
  String dueLine = "";
  while (millis() - startWait < 10000) { // wait max 10 seconds
    if (Serial.available()) {
      dueLine = Serial.readStringUntil('\n');
      dueLine.trim();
      if (dueLine.startsWith("DUE:")) {
        break;
      }
    }
  }

  if (!dueLine.startsWith("DUE:")) {
    Serial.println("ERROR: No due amount received, aborting...");
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(3000);
    return;
  }

  int dueAmount = dueLine.substring(4).toInt();

  if (balance >= dueAmount) {
    int newBalance = balance - dueAmount;

    // Prepare new balance string
    char newBalanceStr[16] = {0};
    snprintf(newBalanceStr, sizeof(newBalanceStr), "%d", newBalance);

    byte newBalanceData[16] = {0};
    memcpy(newBalanceData, newBalanceStr, strlen(newBalanceStr));

    if (writeBlock(4, newBalanceData)) {
      // Send payment confirmation with both due amount and new balance
      Serial.print("PAYMENT_SUCCESS:");
      Serial.print("DUE=");
      Serial.print(dueAmount);
      Serial.print(", NEW_BALANCE=");
      Serial.println(newBalance);
    } else {
      Serial.println("ERROR: Failed to update card balance");
    }
  } else {
    // Send insufficient funds message with current balance
    Serial.print("INSUFFICIENT_FUNDS:");
    Serial.print("CURRENT_BALANCE=");
    Serial.print(balance);
    Serial.print(", REQUIRED=");
    Serial.println(dueAmount);
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  delay(3000); // Wait before next card
}
