#include <SPI.h>
#include "RC522.h"
#include <Arduino.h>
#include <avr/boot.h>

/* 3760 bytes free after removing serial */
/* Compared with 5624 bytes it's a huge difference */
//#define S_DEBUG

void setup() {
  cli();
  pinMode(A1, OUTPUT);
  digitalWrite(A1, HIGH);
  SPI.begin();
  RC522 rfid(10);
  rfid.PCD_Init();

#ifdef S_DEBUG
  Serial.begin(115200);
  Serial.println(F("MostUseless Bootloader Reader"));
#endif

  /* Jump to code after timeout */
  /* Timeout doesn't work but lol, the rest of stuff works */
  while (millis() < 1000 && !rfid.PICC_IsNewCardPresent());
  if (millis() >= 1000) {
    digitalWrite(A1, LOW);
    asm("jmp 0");
  }
  while (!rfid.PICC_ReadCardSerial());

  digitalWrite(A1, LOW);

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
  volatile static uint8_t pageBuffer[256];
  memset(pageBuffer, 0xFF, 256);

  /* Generic key */
  RC522::MIFARE_Key key;
  memset(key.keyByte, 0xFF, 6);

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
    byte len = 18;

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
    digitalWrite(A1, HIGH);
    boot_program_page(memptr, pageBuffer);

    /* Finished writing to flash */
    memptr += 256;
    memset(pageBuffer, 0xFF, 256);
  }
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1(); // Stop encryption on PCD
  rfid.PCD_AntennaOff();

  digitalWrite(A1, LOW);
  asm("jmp 0");
}

void boot_program_page(uint32_t page, uint8_t *buf) {
  uint16_t i;
  uint8_t sreg;

  sreg = SREG;
  cli();

  eeprom_busy_wait();

  boot_page_erase(page);
  boot_spm_busy_wait();
  for (i = 0; i < SPM_PAGESIZE; i += 2)
  {
    uint16_t w = *buf++;
    w += (*buf++) << 8;
    boot_page_fill(page + i, w);
  }

  boot_page_write(page); // Store buffer in flash page.
  boot_spm_busy_wait(); // Wait until the memory is written.

  // Reenable RWW-section again. We need this if we want to jump back
  // to the application after bootloading.
  boot_rww_enable();

  // Re-enable interrupts (if they were ever enabled).
  SREG = sreg;
}

void loop() {
}
