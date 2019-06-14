#ifndef RC522_h
#define RC522_h
#include <SPI.h>
#ifndef RC522_SPICLOCK
#define RC522_SPICLOCK SPI_CLOCK_DIV4
#endif

class RC522 {
  public:
    enum PCD_Register : byte {
      CommandReg = 0x01 << 1,
      ComIEnReg = 0x02 << 1,
      DivIEnReg = 0x03 << 1,
      ComIrqReg = 0x04 << 1,
      DivIrqReg = 0x05 << 1,
      ErrorReg = 0x06 << 1,
      Status1Reg = 0x07 << 1,
      Status2Reg = 0x08 << 1,
      FIFODataReg = 0x09 << 1,
      FIFOLevelReg = 0x0A << 1,
      WaterLevelReg = 0x0B << 1,
      ControlReg = 0x0C << 1,
      BitFramingReg = 0x0D << 1,
      CollReg = 0x0E << 1,
      ModeReg = 0x11 << 1,
      TxModeReg = 0x12 << 1,
      RxModeReg = 0x13 << 1,
      TxControlReg = 0x14 << 1,
      TxASKReg = 0x15 << 1,
      TxSelReg = 0x16 << 1,
      RxSelReg = 0x17 << 1,
      RxThresholdReg = 0x18 << 1,
      DemodReg = 0x19 << 1,
      MfTxReg = 0x1C << 1,
      MfRxReg = 0x1D << 1,
      SerialSpeedReg = 0x1F << 1,
      CRCResultRegH = 0x21 << 1,
      CRCResultRegL = 0x22 << 1,
      ModWidthReg = 0x24 << 1,
      RFCfgReg = 0x26 << 1,
      GsNReg = 0x27 << 1,
      CWGsPReg = 0x28 << 1,
      ModGsPReg = 0x29 << 1,
      TModeReg = 0x2A << 1,
      TPrescalerReg = 0x2B << 1,
      TReloadRegH = 0x2C << 1,
      TReloadRegL = 0x2D << 1,
      TCounterValueRegH = 0x2E << 1,
      TCounterValueRegL = 0x2F << 1,
      TestSel1Reg = 0x31 << 1,
      TestSel2Reg = 0x32 << 1,
      TestPinEnReg = 0x33 << 1,
      TestPinValueReg = 0x34 << 1,
      TestBusReg = 0x35 << 1,
      AutoTestReg = 0x36 << 1,
      VersionReg = 0x37 << 1,
      AnalogTestReg = 0x38 << 1,
      TestDAC1Reg = 0x39 << 1,
      TestDAC2Reg = 0x3A << 1,
      TestADCReg = 0x3B << 1
    };

    enum PCD_Command : byte {
      PCD_Idle = 0x00,
      PCD_Mem = 0x01,
      PCD_GenerateRandomID = 0x02,
      PCD_CalcCRC = 0x03,
      PCD_Transmit = 0x04,
      PCD_NoCmdChange = 0x07,
      PCD_Receive = 0x08,
      PCD_Transceive = 0x0C,
      PCD_MFAuthent = 0x0E,
      PCD_SoftReset = 0x0F
    };

    enum PICC_Command : byte {
      PICC_CMD_REQA = 0x26,
      PICC_CMD_WUPA = 0x52,
      PICC_CMD_CT = 0x88,
      PICC_CMD_SEL_CL1 = 0x93,
      PICC_CMD_SEL_CL2 = 0x95,
      PICC_CMD_SEL_CL3 = 0x97,
      PICC_CMD_HLTA = 0x50,
      PICC_CMD_RATS = 0xE0,
      PICC_CMD_MF_AUTH_KEY_A = 0x60,
      PICC_CMD_MF_AUTH_KEY_B = 0x61,
      PICC_CMD_MF_READ = 0x30,
      PICC_CMD_MF_WRITE = 0xA0,
      PICC_CMD_MF_DECREMENT = 0xC0,
      PICC_CMD_MF_INCREMENT = 0xC1,
      PICC_CMD_MF_RESTORE = 0xC2,
      PICC_CMD_MF_TRANSFER = 0xB0,
      PICC_CMD_UL_WRITE = 0xA2
    };

    enum MIFARE_Misc {
      MF_ACK = 0xA,
      MF_KEY_SIZE = 6
    };

    enum StatusCode : byte {
      STATUS_OK ,
      STATUS_ERROR ,
      STATUS_COLLISION ,
      STATUS_TIMEOUT ,
      STATUS_NO_ROOM ,
      STATUS_INTERNAL_ERROR ,
      STATUS_INVALID ,
      STATUS_CRC_WRONG ,
      STATUS_MIFARE_NACK = 0xff
    };
    typedef struct {
      byte size;
      byte uidByte[10];
      byte sak;
    } Uid;
    typedef struct {
      byte keyByte[MF_KEY_SIZE];
    } MIFARE_Key;
    Uid uid;
    RC522(byte chipSelectPin);
    void PCD_WriteRegister(PCD_Register reg, byte value);
    void PCD_WriteRegister(PCD_Register reg, byte count, byte *values);
    byte PCD_ReadRegister(PCD_Register reg);
    void PCD_ReadRegister(PCD_Register reg, byte count, byte *values, byte rxAlign = 0);

    void PCD_SetRegisterBitMask(PCD_Register reg, byte mask);
    void PCD_ClearRegisterBitMask(PCD_Register reg, byte mask);
    StatusCode PCD_CalculateCRC(byte *data, byte length, byte *result);
    void PCD_Init();
    void PCD_AntennaOff();
    void PCD_SoftPowerDown();
    StatusCode PCD_TransceiveData(byte *sendData, byte sendLen, byte *backData, byte *backLen, byte *validBits = nullptr, byte rxAlign = 0, bool checkCRC = false);
    StatusCode PCD_CommunicateWithPICC(byte command, byte waitIRq, byte *sendData, byte sendLen, byte *backData = nullptr, byte *backLen = nullptr, byte *validBits = nullptr, byte rxAlign = 0, bool checkCRC = false);
    StatusCode PICC_RequestA(byte *bufferATQA, byte *bufferSize);
    StatusCode PICC_WakeupA(byte *bufferATQA, byte *bufferSize);
    StatusCode PICC_REQA_or_WUPA(byte command, byte *bufferATQA, byte *bufferSize);
    virtual StatusCode PICC_Select(Uid *uid, byte validBits = 0);
    StatusCode PICC_HaltA();
    StatusCode PCD_Authenticate(byte command, byte blockAddr, MIFARE_Key *key, Uid *uid);
    void PCD_StopCrypto1();
    StatusCode MIFARE_Read(byte blockAddr, byte *buffer, byte *bufferSize);
    virtual bool PICC_IsNewCardPresent();
    virtual bool PICC_ReadCardSerial();
  protected:
    byte _chipSelectPin;
};
#endif
