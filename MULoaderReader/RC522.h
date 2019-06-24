#ifndef RC522_h
#define RC522_h
#include <avr/io.h>
#define sei()  __asm__ __volatile__ ("sei" ::)
#define cli()  __asm__ __volatile__ ("cli" ::)

class RC522 {
  public:
    enum PCD_Register : uint8_t {
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

    enum PCD_Command : uint8_t {
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

    enum PICC_Command : uint8_t {
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

    enum PICC_Type : uint8_t {
      PICC_TYPE_UNKNOWN,
      PICC_TYPE_ISO_14443_4,
      PICC_TYPE_ISO_18092,
      PICC_TYPE_MIFARE_MINI,
      PICC_TYPE_MIFARE_1K,
      PICC_TYPE_MIFARE_4K,
      PICC_TYPE_MIFARE_UL,
      PICC_TYPE_MIFARE_PLUS,
      PICC_TYPE_MIFARE_DESFIRE,
      PICC_TYPE_TNP3XXX,
      PICC_TYPE_NOT_COMPLETE  = 0xff
    };

    enum StatusCode : uint8_t {
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
      uint8_t size;
      uint8_t uiduint8_t[10];
      uint8_t sak;
    } Uid;

    typedef struct {
      uint8_t keyuint8_t[6];
    } MIFARE_Key;

    Uid uid;
    RC522();

    /* SPI Toolkit */
    void select(bool);
    void beginTransaction() __attribute__((__always_inline__));
    uint8_t transfer(uint8_t);
    void endTransaction();

    static PICC_Type PICC_GetType(uint8_t);

    void PCD_WriteRegister(PCD_Register reg, uint8_t value);
    void PCD_WriteRegister(PCD_Register reg, uint8_t count, uint8_t *values);
    uint8_t PCD_ReadRegister(PCD_Register reg);
    void PCD_ReadRegister(PCD_Register reg, uint8_t count, uint8_t *values, uint8_t rxAlign = 0);

    void PCD_SetRegisterBitMask(PCD_Register reg, uint8_t mask);
    void PCD_ClearRegisterBitMask(PCD_Register reg, uint8_t mask);
    StatusCode PCD_CalculateCRC(uint8_t *data, uint8_t length, uint8_t *result);
    void PCD_Init();
    void PCD_AntennaOff();
    void PCD_SoftPowerDown();
    StatusCode PCD_TransceiveData(uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint8_t *backLen, uint8_t *validBits = nullptr, uint8_t rxAlign = 0, bool checkCRC = false);
    StatusCode PCD_CommunicateWithPICC(uint8_t command, uint8_t waitIRq, uint8_t *sendData, uint8_t sendLen, uint8_t *backData = nullptr, uint8_t *backLen = nullptr, uint8_t *validBits = nullptr, uint8_t rxAlign = 0, bool checkCRC = false);
    StatusCode PICC_RequestA(uint8_t *bufferATQA, uint8_t *bufferSize);
    StatusCode PICC_WakeupA(uint8_t *bufferATQA, uint8_t *bufferSize);
    StatusCode PICC_REQA_or_WUPA(uint8_t command, uint8_t *bufferATQA, uint8_t *bufferSize);
    virtual StatusCode PICC_Select(Uid *uid, uint8_t validBits = 0);
    StatusCode PICC_HaltA();
    StatusCode PCD_Authenticate(uint8_t command, uint8_t blockAddr, MIFARE_Key *key, Uid *uid);
    void PCD_StopCrypto1();
    StatusCode MIFARE_Read(uint8_t blockAddr, uint8_t *buffer, uint8_t *bufferSize);
    virtual bool PICC_IsNewCardPresent();
    virtual bool PICC_ReadCardSerial();
};
#endif
