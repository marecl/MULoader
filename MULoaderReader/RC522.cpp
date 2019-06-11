#include <Arduino.h>
#include "RC522.h"

RC522::RC522(byte chipSelectPin) {
  _chipSelectPin = chipSelectPin;
}

void RC522::PCD_WriteRegister(PCD_Register reg, byte value) {
  SPI.beginTransaction(SPISettings(RC522_SPICLOCK, MSBFIRST, SPI_MODE0));
  digitalWrite(_chipSelectPin, LOW);
  SPI.transfer(reg);
  SPI.transfer(value);
  digitalWrite(_chipSelectPin, HIGH);
  SPI.endTransaction();
}

void RC522::PCD_WriteRegister(PCD_Register reg, byte count, byte *values) {
  SPI.beginTransaction(SPISettings(RC522_SPICLOCK, MSBFIRST, SPI_MODE0));
  digitalWrite(_chipSelectPin, LOW);
  SPI.transfer(reg);
  for (byte index = 0; index < count; index++) {
    SPI.transfer(values[index]);
  }
  digitalWrite(_chipSelectPin, HIGH);
  SPI.endTransaction();
}

byte RC522::PCD_ReadRegister(PCD_Register reg) {
  byte value;
  SPI.beginTransaction(SPISettings(RC522_SPICLOCK, MSBFIRST, SPI_MODE0));
  digitalWrite(_chipSelectPin, LOW);
  SPI.transfer(0x80 | reg);
  value = SPI.transfer(0);
  digitalWrite(_chipSelectPin, HIGH);
  SPI.endTransaction();
  return value;
}

void RC522::PCD_ReadRegister(PCD_Register reg, byte count, byte *values, byte rxAlign) {
  if (count == 0)
    return;

  byte address = 0x80 | reg;
  byte index = 0;
  SPI.beginTransaction(SPISettings(RC522_SPICLOCK, MSBFIRST, SPI_MODE0));
  digitalWrite(_chipSelectPin, LOW);
  count--;
  SPI.transfer(address);
  if (rxAlign) {
    byte mask = (0xFF << rxAlign) & 0xFF;
    byte value = SPI.transfer(address);
    values[0] = (values[0] & ~mask) | (value & mask);
    index++;
  }
  while (index < count) {
    values[index] = SPI.transfer(address);
    index++;
  }
  values[index] = SPI.transfer(0);
  digitalWrite(_chipSelectPin, HIGH);
  SPI.endTransaction();
}

void RC522::PCD_SetRegisterBitMask(PCD_Register reg, byte mask) {
  byte tmp;
  tmp = PCD_ReadRegister(reg);
  PCD_WriteRegister(reg, tmp | mask);
}

void RC522::PCD_ClearRegisterBitMask(PCD_Register reg, byte mask) {
  byte tmp;
  tmp = PCD_ReadRegister(reg);
  PCD_WriteRegister(reg, tmp & (~mask));
}

RC522::StatusCode RC522::PCD_CalculateCRC(byte *data, byte length, byte *result) {
  PCD_WriteRegister(CommandReg, PCD_Idle);
  PCD_WriteRegister(DivIrqReg, 0x04);
  PCD_WriteRegister(FIFOLevelReg, 0x80);
  PCD_WriteRegister(FIFODataReg, length, data);
  PCD_WriteRegister(CommandReg, PCD_CalcCRC);
  for (uint16_t i = 5000; i > 0; i--) {
    byte n = PCD_ReadRegister(DivIrqReg);
    if (n & 0x04) {
      PCD_WriteRegister(CommandReg, PCD_Idle);
      result[0] = PCD_ReadRegister(CRCResultRegL);
      result[1] = PCD_ReadRegister(CRCResultRegH);
      return STATUS_OK;
    }
  }
  return STATUS_TIMEOUT;
}

void RC522::PCD_Init() {
  pinMode(_chipSelectPin, OUTPUT);
  digitalWrite(_chipSelectPin, HIGH);

  /* Reader is wired to RESET line, so we can start config now */

  PCD_WriteRegister(TxModeReg, 0x00);
  PCD_WriteRegister(RxModeReg, 0x00);
  PCD_WriteRegister(ModWidthReg, 0x26);
  PCD_WriteRegister(TModeReg, 0x80);
  PCD_WriteRegister(TPrescalerReg, 0xA9);
  PCD_WriteRegister(TReloadRegH, 0x03);
  PCD_WriteRegister(TReloadRegL, 0xE8);
  PCD_WriteRegister(TxASKReg, 0x40);
  PCD_WriteRegister(ModeReg, 0x3D);

  /* Antenna On */
  byte value = PCD_ReadRegister(TxControlReg);
  if ((value & 0x03) != 0x03) {
    PCD_WriteRegister(TxControlReg, value | 0x03);
  }
}

void RC522::PCD_AntennaOff() {
  PCD_ClearRegisterBitMask(TxControlReg, 0x03);
}

void RC522::PCD_SoftPowerDown() {
  byte val = PCD_ReadRegister(CommandReg);
  val |= (1 << 4);
  PCD_WriteRegister(CommandReg, val);
}

RC522::StatusCode RC522::PCD_TransceiveData(byte *sendData,
    byte sendLen,
    byte *backData,
    byte *backLen,
    byte *validBits,
    byte rxAlign,
    bool checkCRC) {
  byte waitIRq = 0x30;
  return PCD_CommunicateWithPICC(PCD_Transceive, waitIRq, sendData, sendLen, backData, backLen, validBits, rxAlign, checkCRC);
}

RC522::StatusCode RC522::PCD_CommunicateWithPICC(byte command,
    byte waitIRq,
    byte *sendData,
    byte sendLen,
    byte *backData,
    byte *backLen,
    byte *validBits,
    byte rxAlign,
    bool checkCRC) {
  byte txLastBits = validBits ? *validBits : 0;
  byte bitFraming = (rxAlign << 4) + txLastBits;
  PCD_WriteRegister(CommandReg, PCD_Idle);
  PCD_WriteRegister(ComIrqReg, 0x7F);
  PCD_WriteRegister(FIFOLevelReg, 0x80);
  PCD_WriteRegister(FIFODataReg, sendLen, sendData);
  PCD_WriteRegister(BitFramingReg, bitFraming);
  PCD_WriteRegister(CommandReg, command);
  if (command == PCD_Transceive) {
    PCD_SetRegisterBitMask(BitFramingReg, 0x80);
  }
  uint16_t i;
  for (i = 2000; i > 0; i--) {
    byte n = PCD_ReadRegister(ComIrqReg);
    if (n & waitIRq) {
      break;
    }
    if (n & 0x01) {
      return STATUS_TIMEOUT;
    }
  }
  if (i == 0) {
    return STATUS_TIMEOUT;
  }
  byte errorRegValue = PCD_ReadRegister(ErrorReg);
  if (errorRegValue & 0x13) {
    return STATUS_ERROR;
  }
  byte _validBits = 0;
  if (backData && backLen) {
    byte n = PCD_ReadRegister(FIFOLevelReg);
    if (n > *backLen) {
      return STATUS_NO_ROOM;
    }
    *backLen = n;
    PCD_ReadRegister(FIFODataReg, n, backData, rxAlign);
    _validBits = PCD_ReadRegister(ControlReg) & 0x07;
    if (validBits) {
      *validBits = _validBits;
    }
  }
  if (errorRegValue & 0x08) {
    return STATUS_COLLISION;
  }
  if (backData && backLen && checkCRC) {
    if (*backLen == 1 && _validBits == 4) {
      return STATUS_MIFARE_NACK;
    }
    if (*backLen < 2 || _validBits != 0) {
      return STATUS_CRC_WRONG;
    }
    byte controlBuffer[2];
    RC522::StatusCode status = PCD_CalculateCRC(&backData[0], *backLen - 2, &controlBuffer[0]);
    if (status != STATUS_OK) {
      return status;
    }
    if ((backData[*backLen - 2] != controlBuffer[0]) || (backData[*backLen - 1] != controlBuffer[1])) {
      return STATUS_CRC_WRONG;
    }
  }
  return STATUS_OK;
}

RC522::StatusCode RC522::PICC_RequestA(byte *bufferATQA, byte *bufferSize) {
  return PICC_REQA_or_WUPA(PICC_CMD_REQA, bufferATQA, bufferSize);
}

RC522::StatusCode RC522::PICC_WakeupA(byte *bufferATQA, byte *bufferSize) {
  return PICC_REQA_or_WUPA(PICC_CMD_WUPA, bufferATQA, bufferSize);
}

RC522::StatusCode RC522::PICC_REQA_or_WUPA(byte command, byte *bufferATQA, byte *bufferSize) {
  byte validBits;
  RC522::StatusCode status;
  if (bufferATQA == nullptr || *bufferSize < 2)
    return STATUS_NO_ROOM;

  PCD_ClearRegisterBitMask(CollReg, 0x80);
  validBits = 7;
  status = PCD_TransceiveData(&command, 1, bufferATQA, bufferSize, &validBits);
  if (status != STATUS_OK)
    return status;

  if (*bufferSize != 2 || validBits != 0)
    return STATUS_ERROR;

  return STATUS_OK;
}

RC522::StatusCode RC522::PICC_Select(Uid *uid, byte validBits) {
  bool uidComplete;
  bool selectDone;
  bool useCascadeTag;
  byte cascadeLevel = 1;
  RC522::StatusCode result;
  byte count;
  byte checkBit;
  byte index;
  byte uidIndex;
  int8_t currentLevelKnownBits;
  byte buffer[9];
  byte bufferUsed;
  byte rxAlign;
  byte txLastBits;
  byte *responseBuffer;
  byte responseLength;
  if (validBits > 80) {
    return STATUS_INVALID;
  }
  PCD_ClearRegisterBitMask(CollReg, 0x80);
  uidComplete = false;
  while (!uidComplete) {
    switch (cascadeLevel) {
      case 1:
        buffer[0] = PICC_CMD_SEL_CL1;
        uidIndex = 0;
        useCascadeTag = validBits && uid->size > 4;
        break;
      case 2:
        buffer[0] = PICC_CMD_SEL_CL2;
        uidIndex = 3;
        useCascadeTag = validBits && uid->size > 7;
        break;
      case 3:
        buffer[0] = PICC_CMD_SEL_CL3;
        uidIndex = 6;
        useCascadeTag = false;
        break;
      default:
        return STATUS_INTERNAL_ERROR;
        break;
    }
    currentLevelKnownBits = validBits - (8 * uidIndex);
    if (currentLevelKnownBits < 0) {
      currentLevelKnownBits = 0;
    }
    index = 2;
    if (useCascadeTag) {
      buffer[index++] = PICC_CMD_CT;
    }
    byte bytesToCopy = currentLevelKnownBits / 8 + (currentLevelKnownBits % 8 ? 1 : 0);
    if (bytesToCopy) {
      byte maxBytes = useCascadeTag ? 3 : 4;
      if (bytesToCopy > maxBytes) {
        bytesToCopy = maxBytes;
      }
      for (count = 0; count < bytesToCopy; count++) {
        buffer[index++] = uid->uidByte[uidIndex + count];
      }
    }
    if (useCascadeTag) {
      currentLevelKnownBits += 8;
    }
    selectDone = false;
    while (!selectDone) {
      if (currentLevelKnownBits >= 32) {
        buffer[1] = 0x70;
        buffer[6] = buffer[2] ^ buffer[3] ^ buffer[4] ^ buffer[5];
        result = PCD_CalculateCRC(buffer, 7, &buffer[7]);
        if (result != STATUS_OK) {
          return result;
        }
        txLastBits = 0;
        bufferUsed = 9;
        responseBuffer = &buffer[6];
        responseLength = 3;
      }
      else {
        txLastBits = currentLevelKnownBits % 8;
        count = currentLevelKnownBits / 8;
        index = 2 + count;
        buffer[1] = (index << 4) + txLastBits;
        bufferUsed = index + (txLastBits ? 1 : 0);
        responseBuffer = &buffer[index];
        responseLength = sizeof(buffer) - index;
      }
      rxAlign = txLastBits;
      PCD_WriteRegister(BitFramingReg, (rxAlign << 4) + txLastBits);
      result = PCD_TransceiveData(buffer, bufferUsed, responseBuffer, &responseLength, &txLastBits, rxAlign);
      if (result == STATUS_COLLISION) {
        byte valueOfCollReg = PCD_ReadRegister(CollReg);
        if (valueOfCollReg & 0x20) {
          return STATUS_COLLISION;
        }
        byte collisionPos = valueOfCollReg & 0x1F;
        if (collisionPos == 0) {
          collisionPos = 32;
        }
        if (collisionPos <= currentLevelKnownBits) {
          return STATUS_INTERNAL_ERROR;
        }
        currentLevelKnownBits = collisionPos;
        count = currentLevelKnownBits % 8;
        checkBit = (currentLevelKnownBits - 1) % 8;
        index = 1 + (currentLevelKnownBits / 8) + (count ? 1 : 0);
        buffer[index] |= (1 << checkBit);
      }
      else if (result != STATUS_OK) {
        return result;
      }
      else {
        if (currentLevelKnownBits >= 32) {
          selectDone = true;
        }
        else {
          currentLevelKnownBits = 32;
        }
      }
    }
    index = (buffer[2] == PICC_CMD_CT) ? 3 : 2;
    bytesToCopy = (buffer[2] == PICC_CMD_CT) ? 3 : 4;
    for (count = 0; count < bytesToCopy; count++) {
      uid->uidByte[uidIndex + count] = buffer[index++];
    }
    if (responseLength != 3 || txLastBits != 0) {
      return STATUS_ERROR;
    }
    result = PCD_CalculateCRC(responseBuffer, 1, &buffer[2]);
    if (result != STATUS_OK) {
      return result;
    }
    if ((buffer[2] != responseBuffer[1]) || (buffer[3] != responseBuffer[2])) {
      return STATUS_CRC_WRONG;
    }
    if (responseBuffer[0] & 0x04) {
      cascadeLevel++;
    }
    else {
      uidComplete = true;
      uid->sak = responseBuffer[0];
    }
  }
  uid->size = 3 * cascadeLevel + 1;
  return STATUS_OK;
}

RC522::StatusCode RC522::PICC_HaltA() {
  RC522::StatusCode result;
  byte buffer[4];
  buffer[0] = PICC_CMD_HLTA;
  buffer[1] = 0;
  result = PCD_CalculateCRC(buffer, 2, &buffer[2]);
  if (result != STATUS_OK) {
    return result;
  }
  result = PCD_TransceiveData(buffer, sizeof(buffer), nullptr, 0);
  if (result == STATUS_TIMEOUT) {
    return STATUS_OK;
  }
  if (result == STATUS_OK) {
    return STATUS_ERROR;
  }
  return result;
}

RC522::StatusCode RC522::PCD_Authenticate(byte command, byte blockAddr, MIFARE_Key *key, Uid *uid) {
  byte waitIRq = 0x10;
  byte sendData[12];
  sendData[0] = command;
  sendData[1] = blockAddr;
  for (byte i = 0; i < MF_KEY_SIZE; i++) {
    sendData[2 + i] = key->keyByte[i];
  }
  for (byte i = 0; i < 4; i++) {
    sendData[8 + i] = uid->uidByte[i + uid->size - 4];
  }
  return PCD_CommunicateWithPICC(PCD_MFAuthent, waitIRq, &sendData[0], sizeof(sendData));
}

void RC522::PCD_StopCrypto1() {
  PCD_ClearRegisterBitMask(Status2Reg, 0x08);
}

RC522::StatusCode RC522::MIFARE_Read(byte blockAddr, byte *buffer, byte *bufferSize) {
  RC522::StatusCode result;
  if (buffer == nullptr || *bufferSize < 18) {
    return STATUS_NO_ROOM;
  }
  buffer[0] = PICC_CMD_MF_READ;
  buffer[1] = blockAddr;
  result = PCD_CalculateCRC(buffer, 2, &buffer[2]);
  if (result != STATUS_OK) {
    return result;
  }
  return PCD_TransceiveData(buffer, 4, buffer, bufferSize, nullptr, 0, true);
}

bool RC522::PICC_IsNewCardPresent() {
  byte bufferATQA[2];
  byte bufferSize = sizeof(bufferATQA);
  PCD_WriteRegister(TxModeReg, 0x00);
  PCD_WriteRegister(RxModeReg, 0x00);
  PCD_WriteRegister(ModWidthReg, 0x26);
  RC522::StatusCode result = PICC_RequestA(bufferATQA, &bufferSize);
  return (result == STATUS_OK || result == STATUS_COLLISION);
}

bool RC522::PICC_ReadCardSerial() {
  RC522::StatusCode result = PICC_Select(&uid);
  return (result == STATUS_OK);
}
