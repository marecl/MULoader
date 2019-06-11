#include <SPI.h>
#include <MFRC522.h>

MFRC522 rfid(10, 9);

byte prog[] = {0xE0, 0x01, 0xB9, 0x07, 0xB9, 0x08, 0xCF, 0xFF
              };

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  rfid.PCD_DumpVersionToSerial();
  Serial.println("Ready to write some shit");
  Serial.flush();
}

void loop() {
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  if (!rfid.PICC_IsNewCardPresent())
    return;

  if (!rfid.PICC_ReadCardSerial())
    return;


  Serial.print(F("Card UID:"));    //Dump UID
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.print(F("\r\nPICC type: "));   // Dump PICC type
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.println(rfid.PICC_GetTypeName(piccType));

  uint16_t s = sizeof(prog);
  uint8_t blocks = (s / 16);
  if (s % 16 != 0) blocks += 1;
  Serial.print("Sketch size:\t\t");
  Serial.println(s);
  Serial.print("Total blocks:\t\t");
  Serial.println(blocks);
  if (blocks > 46) { //Excluding trailer,0 and 1 block
    Serial.println("Sketch too big!");
    return;
  }

  MFRC522::StatusCode status;
  uint32_t offset = 0;
  for (uint8_t block = 1; offset < blocks; block++) { //Blocks
    if ((block + 1) % 4 == 0)
      continue;

    status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(rfid.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.print(block);
      Serial.print(F(" PCD_Authenticate() failed: "));
      Serial.println(rfid.GetStatusCodeName(status));
      return;
    }

    byte buffer[16];
    memset(buffer, 0, 16);

    if (block > 1) {
      for (uint8_t a = 0; a < (((s - (16 * offset)) > 16) ? 16 : ((s % 16) == 0 ? 16 : s % 16)); a++)
        buffer[a] = prog[(16 * offset) + a];
      offset++;
    } else if (block == 1) {
      Serial.println("Writing sketch metadata");
      buffer[0] = s & 0xFF;
      buffer[1] = s >> 8;
    }

    status = rfid.MIFARE_Write(block, buffer, 16);

    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("MIFARE_Write() failed: "));
      Serial.println(rfid.GetStatusCodeName(status));
      Serial.println(block);
      return;
    }
  }

  Serial.println("Done!");
  rfid.PICC_DumpToSerial(&(rfid.uid));
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();  // Stop encryption on PCD
}
