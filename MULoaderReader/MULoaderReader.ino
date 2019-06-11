#include <SPI.h>
#include "RC522.h"
#include <avr/boot.h>

/* Move this to AVR Studio */

#define S_DEBUG

void setup() {
  RC522 rfid(10);
  SPI.begin();
#ifdef S_DEBUG
  Serial.begin(2000000);
  Serial.println(F("MostUseless Bootloader Reader"));
#endif
  rfid.PCD_Init();

  RC522::MIFARE_Key key;
  memset(key.keyByte, 0xFF, 6);


  /* Jump to code after timeout */
  while (millis() < 10000 && !rfid.PICC_IsNewCardPresent());
  if (millis() > 10000) asm("jmp 0x0000");

  while (!rfid.PICC_ReadCardSerial());

  /* Sketch metadata */
  uint16_t siz = 0;
  uint8_t parts = 0;
  uint8_t pages = 0;
  uint8_t pageExtra = 0;
  uint8_t blocks = 2;
  uint8_t blockExtra = 0;

  /* Buffer and SPM stuff */
  uint8_t pageoffset = 0;
  uint8_t memptr = 0;

  /*
      We don't need to worry about extra data in last
      page if we already set buffer as empty :)
  */
  uint8_t pageBuffer[256];
  memset(pageBuffer, 0xFF, 256);

  /*
      Reading every block and using page offset
      to save progress if more parts are needed
  */

  for (uint8_t block = 1; block < blocks; block++) {
    if ((block + 1) % 4 == 0) {
      blocks++;
      continue;
    }

    uint8_t buffer[18];
    const byte len = 18;

    rfid.PCD_Authenticate(RC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(rfid.uid));
    rfid.MIFARE_Read(block, buffer, &len);

    /*
        2 bytes - code size
        1 byte - parts [token]
        1 byte - full pages (including not full)
        1 byte - amount of data in last page
        1 byte - full blocks
        1 byte - amount of data in last block
    */
    if (block == 1) {
      siz = word(buffer[0], buffer[1]);
      parts = buffer[2];
      pages = buffer[3];
      pageExtra = buffer[4];
      blocks = buffer[5] + 2;
      blockExtra = buffer[6];
#ifdef S_DEBUG
      Serial.print(F("Sketch size:\t"));
      Serial.print(siz);
      Serial.print(F(" bytes\r\nParts [tokens]:\t"));
      Serial.println(parts);
      Serial.print(F("Pages total:\t"));
      Serial.println(pages);
      Serial.print(F("In last page:\t"));
      Serial.print(pageExtra);
      Serial.println(F(" bytes"));
      Serial.print(F("Blocks total:\t"));
      Serial.println(blocks - 2);
      Serial.print(F("In last block:\t"));
      Serial.print(blockExtra);
      Serial.println(F(" bytes"));
#endif
      continue;
    }

    for (uint8_t a = 0; a < 16; a++) {
      pageBuffer[(pageoffset * 16) + a] = buffer[a];
    }
    pageoffset++;

    /* Check if page is full */
    if (pageoffset < 16 && block != blocks - 1) continue;
    pageoffset = 0;

#ifdef S_DEBUG
    Serial.print(F("\r\n ---- Page "));
    Serial.print(memptr + 1);
    Serial.println(F(" ----"));
    for (uint16_t x = 1; x <= 256; x++) {
      Serial.print(pageBuffer[x - 1] < 0x10 ? " 0" : " ");
      Serial.print(pageBuffer[x - 1], HEX);
      if (x % 16 == 0)
        Serial.println();
    }
#endif

    /* Write each page to flash */
    /* 3888 bytes free after removing serial */



    /* Finished writing to flash */
    memptr++;
    memset(pageBuffer, 0xFF, 256);
  }
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1(); // Stop encryption on PCD

  asm("jmp 0x0000");
}

void loop() {
}
