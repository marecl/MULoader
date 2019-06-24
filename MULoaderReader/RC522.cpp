#include "RC522.h"

RC522::RC522() {
  this->select(false);
}

/* Chip Select on D10 */
void RC522::select(bool on) {
  if (!on) PORTB |= 0b000100;
  else PORTB &= 0b111011;
}

void RC522::beginTransaction() {
  /*
    SPISettings in a nutshell:
    Clock: 4MHz
    MSBFIRST
    SPI_MODE0
  */
  SPCR = _BV(SPE) | _BV(MSTR);
  SPSR = 0x00;
}

uint8_t RC522::transfer(uint8_t data) {
  SPDR = data;
  asm volatile("nop");
  while (!(SPSR & _BV(SPIF)));
  return SPDR;
}

void RC522::endTransaction() {
  uint8_t sreg = SREG;
  cli();
  SPCR &= ~_BV(SPE);
  SREG = sreg;
}

RC522::PICC_Type RC522::PICC_GetType(uint8_t sak) {
  sak &= 0x7F;
  switch (sak) {
    case 0x04: return PICC_TYPE_NOT_COMPLETE;
    case 0x09: return PICC_TYPE_MIFARE_MINI;
    case 0x08: return PICC_TYPE_MIFARE_1K;
    case 0x18: return PICC_TYPE_MIFARE_4K;
    case 0x00: return PICC_TYPE_MIFARE_UL;
    case 0x10:
    case 0x11: return PICC_TYPE_MIFARE_PLUS;
    case 0x01: return PICC_TYPE_TNP3XXX;
    case 0x20: return PICC_TYPE_ISO_14443_4;
    case 0x40: return PICC_TYPE_ISO_18092;
    default: return PICC_TYPE_UNKNOWN;
  }
}

void RC522::PCD_WriteRegister(PCD_Register reg, uint8_t value) {
  this->beginTransaction();
  this->select(true);
  this->transfer(reg);
  this->transfer(value);
  this->select(false);
  this->endTransaction();
}

void RC522::PCD_WriteRegister(PCD_Register reg, uint8_t count, uint8_t *values) {
  this->beginTransaction();
  this->select(true);
  this->transfer(reg);
  for (uint8_t index = 0; index < count; index++) {
    this->transfer(values[index]);
  }
  this->select(false);
  this->endTransaction();
}

uint8_t RC522::PCD_ReadRegister(PCD_Register reg) {
  uint8_t value;
  this->beginTransaction();
  this->select(true);
  this->transfer(0x80 | reg);
  value = this->transfer(0);
  this->select(false);
  this->endTransaction();
  return value;
}

void RC522::PCD_ReadRegister(PCD_Register reg, uint8_t count, uint8_t *values, uint8_t rxAlign) {
  if (count == 0)
    return;

  uint8_t address = 0x80 | reg;
  uint8_t index = 0;
  this->beginTransaction();
  this->select(true);
  count--;
  this->transfer(address);
  if (rxAlign) {
    uint8_t mask = (0xFF << rxAlign) & 0xFF;
    uint8_t value = this->transfer(address);
    values[0] = (values[0] & ~mask) | (value & mask);
    index++;
  }
  while (index < count) {
    values[index] = this->transfer(address);
    index++;
  }
  values[index] = this->transfer(0);
  this->select(false);
  this->endTransaction();
}

void RC522::PCD_SetRegisterBitMask(PCD_Register reg, uint8_t mask) {
  uint8_t tmp;
  tmp = PCD_ReadRegister(reg);
  PCD_WriteRegister(reg, tmp | mask);
}

void RC522::PCD_ClearRegisterBitMask(PCD_Register reg, uint8_t mask) {
  uint8_t tmp;
  tmp = PCD_ReadRegister(reg);
  PCD_WriteRegister(reg, tmp & (~mask));
}

RC522::StatusCode RC522::PCD_CalculateCRC(uint8_t *data, uint8_t length, uint8_t *result) {
  PCD_WriteRegister(CommandReg, PCD_Idle);
  PCD_WriteRegister(DivIrqReg, 0x04);
  PCD_WriteRegister(FIFOLevelReg, 0x80);
  PCD_WriteRegister(FIFODataReg, length, data);
  PCD_WriteRegister(CommandReg, PCD_CalcCRC);
  for (uint16_t i = 5000; i > 0; i--) {
    uint8_t n = PCD_ReadRegister(DivIrqReg);
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
  this->select(false);

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
  uint8_t value = PCD_ReadRegister(TxControlReg);
  if ((value & 0x03) != 0x03) {
    PCD_WriteRegister(TxControlReg, value | 0x03);
  }
}

void RC522::PCD_AntennaOff() {
  PCD_ClearRegisterBitMask(TxControlReg, 0x03);
}

void RC522::PCD_SoftPowerDown() {
  uint8_t val = PCD_ReadRegister(CommandReg);
  val |= (1 << 4);
  PCD_WriteRegister(CommandReg, val);
}

RC522::StatusCode RC522::PCD_TransceiveData(uint8_t *sendData,
    uint8_t sendLen,
    uint8_t *backData,
    uint8_t *backLen,
    uint8_t *validBits,
    uint8_t rxAlign,
    bool checkCRC) {
  uint8_t waitIRq = 0x30;
  return PCD_CommunicateWithPICC(PCD_Transceive, waitIRq, sendData, sendLen, backData, backLen, validBits, rxAlign, checkCRC);
}

RC522::StatusCode RC522::PCD_CommunicateWithPICC(uint8_t command,
    uint8_t waitIRq,
    uint8_t *sendData,
    uint8_t sendLen,
    uint8_t *backData,
    uint8_t *backLen,
    uint8_t *validBits,
    uint8_t rxAlign,
    bool checkCRC) {
  uint8_t txLastBits = validBits ? *validBits : 0;
  uint8_t bitFraming = (rxAlign << 4) + txLastBits;
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
    uint8_t n = PCD_ReadRegister(ComIrqReg);
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
  uint8_t errorRegValue = PCD_ReadRegister(ErrorReg);
  if (errorRegValue & 0x13) {
    return STATUS_ERROR;
  }
  uint8_t _validBits = 0;
  if (backData && backLen) {
    uint8_t n = PCD_ReadRegister(FIFOLevelReg);
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
    uint8_t controlBuffer[2];
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

RC522::StatusCode RC522::PICC_RequestA(uint8_t *bufferATQA, uint8_t *bufferSize) {
  return PICC_REQA_or_WUPA(PICC_CMD_REQA, bufferATQA, bufferSize);
}

RC522::StatusCode RC522::PICC_WakeupA(uint8_t *bufferATQA, uint8_t *bufferSize) {
  return PICC_REQA_or_WUPA(PICC_CMD_WUPA, bufferATQA, bufferSize);
}

RC522::StatusCode RC522::PICC_REQA_or_WUPA(uint8_t command, uint8_t *bufferATQA, uint8_t *bufferSize) {
  uint8_t validBits;
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

RC522::StatusCode RC522::PICC_Select(Uid *uid, uint8_t validBits) {
  bool uidComplete;
  bool selectDone;
  bool useCascadeTag;
  uint8_t cascadeLevel = 1;
  RC522::StatusCode result;
  uint8_t count;
  uint8_t checkBit;
  uint8_t index;
  uint8_t uidIndex;
  int8_t currentLevelKnownBits;
  uint8_t buffer[9];
  uint8_t bufferUsed;
  uint8_t rxAlign;
  uint8_t txLastBits;
  uint8_t *responseBuffer;
  uint8_t responseLength;
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
    uint8_t uint8_tsToCopy = currentLevelKnownBits / 8 + (currentLevelKnownBits % 8 ? 1 : 0);
    if (uint8_tsToCopy) {
      uint8_t maxuint8_ts = useCascadeTag ? 3 : 4;
      if (uint8_tsToCopy > maxuint8_ts) {
        uint8_tsToCopy = maxuint8_ts;
      }
      for (count = 0; count < uint8_tsToCopy; count++) {
        buffer[index++] = uid->uiduint8_t[uidIndex + count];
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
        uint8_t valueOfCollReg = PCD_ReadRegister(CollReg);
        if (valueOfCollReg & 0x20) {
          return STATUS_COLLISION;
        }
        uint8_t collisionPos = valueOfCollReg & 0x1F;
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
    uint8_tsToCopy = (buffer[2] == PICC_CMD_CT) ? 3 : 4;
    for (count = 0; count < uint8_tsToCopy; count++) {
      uid->uiduint8_t[uidIndex + count] = buffer[index++];
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
  uint8_t buffer[4];
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

RC522::StatusCode RC522::PCD_Authenticate(uint8_t command, uint8_t blockAddr, MIFARE_Key *key, Uid *uid) {
  uint8_t waitIRq = 0x10;
  uint8_t sendData[12];
  sendData[0] = command;
  sendData[1] = blockAddr;
  for (uint8_t i = 0; i < 6; i++) {
    sendData[2 + i] = key->keyuint8_t[i];
  }
  for (uint8_t i = 0; i < 4; i++) {
    sendData[8 + i] = uid->uiduint8_t[i + uid->size - 4];
  }
  return PCD_CommunicateWithPICC(PCD_MFAuthent, waitIRq, &sendData[0], sizeof(sendData));
}

void RC522::PCD_StopCrypto1() {
  PCD_ClearRegisterBitMask(Status2Reg, 0x08);
}

RC522::StatusCode RC522::MIFARE_Read(uint8_t blockAddr, uint8_t *buffer, uint8_t *bufferSize) {
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
  uint8_t bufferATQA[2];
  uint8_t bufferSize = sizeof(bufferATQA);
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
