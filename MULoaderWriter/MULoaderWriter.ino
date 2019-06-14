#include <SPI.h>
#include <MFRC522.h>

MFRC522 rfid(10, 9);

/* Maybe load from serial? */
/*
   YES, IT HAS TO BE THIS LONG
*/
byte prog[] = {0x01, 0xE0, 0x07, 0xB9, 0x08, 0xB9, 0xFE, 0xCF
              };

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  rfid.PCD_DumpVersionToSerial();
  Serial.println(F("MostUseless Bootloader Writer"));
  Serial.flush();
}

void loop() {
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  if (!rfid.PICC_IsNewCardPresent())
    return;

  if (!rfid.PICC_ReadCardSerial())
    return;

  Serial.print(F("Card UID:"));
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.print(F("\r\nPICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.println(rfid.PICC_GetTypeName(piccType));

  uint16_t s = sizeof(prog);
  uint8_t blocks = (s / 16);
  if (s % 16 != 0) blocks += 1;

  /* 736 bytes per card */
  if (blocks > 46) {
    Serial.println(F("Parts not implemented yet!"));
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
    memset(buffer, 0xFF, 16);

    if (block > 1) {
      for (uint8_t a = 0; a < (((s - (16 * offset)) > 16) ? 16 : ((s % 16) == 0 ? 16 : s % 16)); a++)
        buffer[a] = prog[(16 * offset) + a];
      offset++;
    } else if (block == 1) {
      Serial.println(F("Writing sketch metadata"));
      /*
          2 bytes - code size
          1 byte - parts [token]
          1 byte - full pages (including not full)
          1 byte - amount of data in last page
          1 byte - full blocks
          1 byte - amount of data in last block
      */
      buffer[0] = s >> 8;
      buffer[1] = s & 0xFF;
      buffer[2] = s / 736 + (s % 736 != 0 ? 1 : 0);
      buffer[3] = s / 256;
      if (buffer[3] != 0 || s < 256) buffer[3] += 1;
      buffer[4] = s % 256;
      buffer[5] = s / 16 + (s % 16 != 0 ? 1 : 0);
      buffer[6] = s % 16;
      Serial.print(F("Sketch size:\t"));
      Serial.print(s);
      Serial.print(F(" bytes\r\nParts [tokens]:\t"));
      Serial.println(buffer[2]);
      Serial.print(F("Pages total:\t"));
      Serial.println(buffer[3]);
      Serial.print(F("In last page:\t"));
      Serial.print(buffer[4]);
      Serial.println(F(" bytes"));
      Serial.print(F("Blocks total:\t"));
      Serial.println(buffer[5]);
      Serial.print(F("In last block:\t"));
      Serial.print(buffer[6]);
      Serial.println(F(" bytes"));
    }

    status = rfid.MIFARE_Write(block, buffer, 16);

    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("MIFARE_Write() failed: "));
      Serial.println(rfid.GetStatusCodeName(status));
      Serial.println(block);
      return;
    }
  }

  Serial.println(F("Done!"));
  Serial.flush();
  delay(100);
  rfid.PICC_DumpToSerial(&(rfid.uid));
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}
