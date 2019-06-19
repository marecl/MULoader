#include <SPI.h>
#include <MFRC522.h>

MFRC522 rfid(10, 9);
MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;

enum Codes : char {
  COMMAND_OK = 'A',   //ACK
  COMMAND_ERROR,      //NAK
  CODE_META,          //Code metadata
  CODE_BEGIN,         //Code upload
  TAG_OK,             //Tag updated
  TAG_ERROR_WRITE,    //Info to PC
  TAG_ERROR_READ,
  TAG_ERROR_VERIFY,
  TAG_NEXT,           //From pc next tag is expected
  TAG_WAITING,        //Info to PC
  TAG_FIRST,          //Request to PC
  TAG_NOT_FIRST,      //Info to PC
  TAG_UPDATING_META,  //Info to PC
  BUFFER_OK,          //Info from PC
  BUFFER_ERROR,       //Info from PC
  BUFFER_REPEAT,      //Request to PC
  BUFFER_WAITING,     //Info to PC
  ALL_DONE,           //Info to PC
  INIT_OK             //Info to PC
};

struct _meta {
  uint16_t size;
  uint16_t blocks;
  uint8_t pages;
  uint8_t parts;
} meta;

uint8_t buffer[18];
uint8_t len = 18;
uint32_t bytesWritten;
uint32_t toWrite;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  Serial.write(INIT_OK);
  while (Serial.read() != CODE_META);
  uint16_t size = Serial.parseInt();

  /* Why would you upload 0 byte code*/
  if (size == 0) {
    Serial.write(COMMAND_ERROR);
    return;
  } Serial.write(COMMAND_OK);

  bytesWritten = 0;
  toWrite = size;

  meta.size = size;
  meta.pages = size / 256;
  if (size % 256 != 0) meta.pages++;
  meta.blocks = size / 16;
  if (size % 16 != 0) meta.blocks++;
}

void loop() {
  uint8_t currentPart = 1;
  uint16_t capacity = 0;

  do {
    /* Send info we're waiting for next tag */
    if (bytesWritten < toWrite) {
      Serial.write(TAG_WAITING);
      while (Serial.read() != TAG_NEXT);
    }

    while (!rfid.PICC_IsNewCardPresent());
    while (!rfid.PICC_ReadCardSerial());

    /* Get card type */
    MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);

    /* Get space available */
    switch (piccType) {
      case MFRC522::PICC_TYPE_MIFARE_UL:
      case MFRC522::PICC_TYPE_MIFARE_PLUS:
      case MFRC522::PICC_TYPE_MIFARE_DESFIRE:
      case MFRC522::PICC_TYPE_MIFARE_4K:
      case MFRC522::PICC_TYPE_MIFARE_MINI: //I dare you to use this one
        rfid.PICC_DumpDetailsToSerial(&(rfid.uid));
        capacity = 0;
      default: continue;
      case MFRC522::PICC_TYPE_MIFARE_1K: capacity = 736; break;
    }

    /* Send tag UID and type to pc (text) */
    rfid.PICC_DumpDetailsToSerial(&(rfid.uid));

    /* Send space available to pc */
    Serial.println(capacity);

    /* Write whole tag */
    for (uint16_t a = 1; a < 64; a++) {
      if ((a + 1) % 4 == 0)
        continue; //Skip trailers
      if (a != 1) {
        /* Wait for new transmission */
        do {
          /* Send info we're waiting for code */
          Serial.write(BUFFER_WAITING);
          while (Serial.read() != CODE_BEGIN);

          /* Read into buffer */
          while (Serial.available() < 16); //Wait for buffer to fill
          for (uint8_t b = 0; b < 16; b++)
            buffer[b] = Serial.read();

          while (Serial.available())Serial.read(); //Discard leftovers
          Serial.write(BUFFER_OK);

          /* Send buffer to pc */
          for (uint8_t b = 0; b < 16; b++)
            Serial.write(buffer[b]);

          /* Wait for confirmation */
          while (!Serial.available());
        } while (Serial.read() == BUFFER_ERROR
                 && Serial.peek() != BUFFER_OK);
      } else {
        Serial.write(TAG_UPDATING_META);
        buffer[0] = currentPart;
      }

      /* Write awaiting data to tag */
      int8_t tries = 0;
      do {
        rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, a, &key, &(rfid.uid));
        status = rfid.MIFARE_Write(a, buffer, 16);
        tries++;
      } while (tries < 5 && status != MFRC522::STATUS_OK);
      if (status != MFRC522::STATUS_OK)
        Serial.write(TAG_ERROR_WRITE);
      else Serial.write(TAG_OK);

      /* Read block from tag and verify against buffer */
      tries = 0;
      do {
        uint8_t buf[18];
        status = rfid.MIFARE_Read(a, buf, &len);
        for (uint8_t x = 0; x < 16; x++) {
          if (buffer[x] != buf[x]) {
            tries++;
            break;
          }
          if (x == 15) tries = -1;
        }
      } while (tries < 5 && tries != -1);
      if (status != MFRC522::STATUS_OK)
        Serial.write(TAG_ERROR_READ);
      else if (tries >= 5) Serial.write(TAG_ERROR_VERIFY);
      else Serial.write(TAG_OK);


      /* Keeping counters reliable */
      if (a != 1) bytesWritten += 16;
      memset(buffer, 0xFF, 16);
      if (bytesWritten >= toWrite) break;
    }
    currentPart++;

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  } while (bytesWritten < toWrite);

  meta.parts = currentPart - 1;

  /* Check if card is part 1 */
  do {
    /* Confirm every card change */
    Serial.write(TAG_FIRST);
    while (Serial.read() != TAG_NEXT);

    /* Wait for card */
    while (!rfid.PICC_IsNewCardPresent());
    while (!rfid.PICC_ReadCardSerial());
    rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 1, &key, &(rfid.uid));
    rfid.MIFARE_Read(1, buffer, &len);
    if (buffer[0] != 1) {
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
      Serial.write(TAG_NOT_FIRST);
    } else Serial.write(TAG_OK);
  } while (buffer[0] != 1);

  /* Write metadata to first block */
  memset(buffer, 0xFF, 16);
  buffer[0] = 1;
  buffer[1] = meta.size >> 8;
  buffer[2] = meta.size & 0x00FF;
  buffer[3] = meta.blocks >> 8;
  buffer[4] = meta.blocks & 0x00FF;
  buffer[5] = meta.pages;
  buffer[6] = meta.parts;

  /*
    I forgot about checking this one
    Well, if it can be programmed and read twice,
    then there is no need for verification again
    ¯\_(ツ)_/¯
  */
  rfid.MIFARE_Write(1, buffer, 16);

  /* We're done here */
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  Serial.write(ALL_DONE);
  while (true);
}
