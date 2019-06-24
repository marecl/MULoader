#include <avr/io.h>
#include <avr/boot.h>
#include "RC522.h"

/* Moving to Atmel Studio? */

void boot_program_page(uint32_t, uint8_t*);
void __attribute__((noinline)) watchdogConfig(uint8_t);
void initSPI();

/* 3820 bytes free after removing serial */
/* Compared with 5714 bytes it's a huge difference */
//#define S_DEBUG

void setup() {
  cli();
  DDRC |= 0b00000010;
  PORTC |= 0b00000010;
#ifdef S_DEBUG
  Serial.begin(115200);
  Serial.println(F("MostUseless Bootloader Reader"));
  Serial.flush();
#endif
  uint8_t ch;
  ch = MCUSR;
  if (ch != 0) {
    if ((ch & (_BV(WDRF) | _BV(EXTRF))) != _BV(EXTRF)) {
      if (ch & _BV(EXTRF))
        MCUSR = ~(_BV(WDRF));

#ifdef S_DEBUG
      Serial.println("Ext reset");
      Serial.flush();
#endif

      watchdogConfig(0);
      PORTC &= !0b00000010;
      asm("jmp 0");
    }
  }
#ifdef S_DEBUG
  Serial.println("Entering bootloader");
  Serial.flush();
#endif

  initSPI();
  RC522 rfid;
  rfid.PCD_Init();

  /* Generic key */
  RC522::MIFARE_Key key;
  memset(key.keyuint8_t, 0xFF, 6);

  /* You have 2 seconds to scan first tag */
  watchdogConfig(_BV(WDP2) | _BV(WDP1) | _BV(WDP0) | _BV(WDE));

  /* If the tag is not scanned, it will triger watchdog */
  while (!rfid.PICC_IsNewCardPresent());
  while (!rfid.PICC_ReadCardSerial());

  /* Now you have 8 seconds between tags */
  watchdogConfig(_BV(WDP3) | _BV(WDP0) | _BV(WDE));

  PORTC &= !0b00000010;

  /* Sketch metadata */
  uint16_t siz = 0;
  uint16_t blocks = 0;
  uint8_t parts = 0;

  /* Programming */
  bool metaRead = false;
  uint8_t currentPart = 0;
  uint16_t blocksWritten = 0;
  volatile static uint8_t buffer[18];
  uint8_t len = 18;

  /* Buffer and SPM stuff */
  volatile static uint8_t writeBuffer[16];
  uint16_t memptr = 0;

  /* Token capacity (blocks) may vary */
  uint16_t tokencap = 0;
  do {
    PORTC |= 0b00000010;
    asm("wdr");
#ifdef S_DEBUG
    Serial.println("Scan tag");
    Serial.flush();
#endif
    /* Check new card */
    while (!rfid.PICC_IsNewCardPresent());
    while (!rfid.PICC_ReadCardSerial());
    RC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
    PORTC &= !0b00000010;

    switch (piccType) {
      case RC522::PICC_TYPE_MIFARE_UL:
      case RC522::PICC_TYPE_MIFARE_PLUS:
      case RC522::PICC_TYPE_MIFARE_DESFIRE:
      case RC522::PICC_TYPE_MIFARE_4K:
      case RC522::PICC_TYPE_MIFARE_MINI: //I dare you to use this one
        tokencap = 0;
      default: continue;
      case RC522::PICC_TYPE_MIFARE_1K:
        tokencap = 64; break;
    }

    if (tokencap == 0) {
#ifdef S_DEBUG
      Serial.println("Not supported");
#endif
      return;
    }

    rfid.PCD_Authenticate(RC522::PICC_CMD_MF_AUTH_KEY_A, 1, &key, &(rfid.uid));
    rfid.MIFARE_Read(1, buffer, &len);

    if (buffer[0] - 1 != currentPart) {
#ifdef S_DEBUG
      Serial.print("Wrong part: ");
      Serial.print(buffer[0]);
      Serial.print(" != ");
      Serial.println(currentPart + 1);
#endif
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
      continue;
    }

    if (!metaRead && currentPart == 0) {
      /*
        2 bytes - code size
        2 bytes - full blocks
        1 byte - pages (unused)
        1 byte - parts [tokens]
      */
      siz = word(buffer[1], buffer[2]);
      blocks = word(buffer[3], buffer[4]);
      parts = buffer[6];
#ifdef S_DEBUG
      Serial.print(F("Sketch size:\t"));
      Serial.print(siz);
      Serial.print(F(" bytes\r\nParts [tokens]:\t"));
      Serial.println(parts);
      Serial.print(F("Blocks total:\t"));
      Serial.println(blocks);
      Serial.flush();
#endif
      metaRead = true;
    }

    for (uint8_t a = 2;
         a < tokencap && blocksWritten != blocks;
         a++) {
      asm("wdr");
      if ((a + 1) % 4 == 0) continue;
#ifdef S_DEBUG
      Serial.print(a);
      Serial.print('\t');
#endif
      rfid.PCD_Authenticate(RC522::PICC_CMD_MF_AUTH_KEY_A, a, &key, &(rfid.uid));
      rfid.MIFARE_Read(a, buffer, &len);
      for (uint16_t x = 0; x < 16; x++)
        writeBuffer[x] = buffer[x];

      PORTC |= 0b00000010;
      boot_program_page(memptr, writeBuffer);
      blocksWritten += 1;
      memptr += 16;
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();

    currentPart += 1;
  } while (currentPart <= parts && blocksWritten != blocks);
  PORTC |= 0b00000010;
#ifdef S_DEBUG
  Serial.println("Done!");
  Serial.flush();
#endif

  /* We're done here */
  rfid.PCD_AntennaOff();
  watchdogConfig(_BV(WDE)); //16ms
  PORTC &= !0b00000010;
  while (1); //Trigger watchdog to jump to code
}

void boot_program_page(uint16_t addr, uint8_t *buf) {
#ifdef S_DEBUG
  Serial.print("Writing at: ");
  Serial.println(addr);
  Serial.print('\t');
  for (uint16_t x = 0; x < 16; x++) {
    Serial.print(buf[x] < 0x10 ? " 0" : " ");
    Serial.print(buf[x], HEX);
  }
  Serial.println();
  Serial.flush();
#endif
  uint8_t sreg;

  sreg = SREG;
  cli();

  eeprom_busy_wait();

  if (addr == 0 || addr % SPM_PAGESIZE == 0)
    boot_page_erase(addr);
  boot_spm_busy_wait();
  for (uint8_t i = 0; i < 16; i += 2)
  {
    uint16_t w = *buf++;
    w += (*buf++) << 8;
    boot_page_fill(addr + i, w);
  }

  boot_page_write(addr);
  boot_spm_busy_wait();
  boot_rww_enable();

  SREG = sreg;
}

void watchdogConfig(uint8_t x) {
  WDTCSR = _BV(WDCE) | _BV(WDE);
  WDTCSR = x;
}

void initSPI() {
  uint8_t sreg = SREG;
  cli();

  /* Set up SPI outs */
  DDRB |= 0b101100;
  /* SPI in */
  DDRB &= 0b101111;

  SPCR |= _BV(MSTR) | _BV(SPE);
  SREG = sreg;
}

void loop() {
}
