#include <SPI.h>
#include "RC522.h"
#include <avr/boot.h>

uint8_t pageBuffer[256];
RC522 rfid(10);

void setup() {
  SPI.begin();
  Serial.begin(2000000);
  Serial.println("XD");
  rfid.PCD_Init();

  RC522::MIFARE_Key key;
  memset(key.keyByte, 0xFF, 6);

  while (millis() < 10000 && !rfid.PICC_IsNewCardPresent());

  if (millis() > 10000) asm("jmp 0x0000");

  while (!rfid.PICC_ReadCardSerial());

  uint8_t blocks = 1;
  uint16_t siz = 0;
  uint16_t flashptr = 0;
  uint8_t pages = 0;
  memset(pageBuffer, 0xFF, 256);

  for (uint8_t block = 1; flashptr < blocks; block++) {
    if ((block + 1) % 4 == 0)
      continue;

    uint8_t buffer[18];

    byte len = 18;

    rfid.PCD_Authenticate(RC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(rfid.uid));
    rfid.MIFARE_Read(block, buffer, &len);

    if (block == 1) {
      siz = word(buffer[0], buffer[1]);
      blocks = siz / 16;
      if (siz % 16 != 0)
        blocks++;
      pages = siz / 256;
      if (siz % 256 != 0)
        pages++;
      Serial.println(siz);
      Serial.println(blocks);
      Serial.println(pages);
      continue;
    }

    /* Write to flash 16 bytes at a time */
    /* 3498 bytes free after removing serial */

    //erasePage(flashptr);

    /* E0 01 B9 07 B9 08 CF FF */

    flashptr++;

    /*for (uint8_t x = 0; x < 16; x++) {
      Serial.print(buffer[x] < 0x10 ? " 0" : " ");
      Serial.print(buffer[x], HEX);
      }
      Serial.println();
    */


    /* Finished writing to flash */

  }
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1(); // Stop encryption on PCD

  asm("jmp 0x0000");
}

void loop() {
}
