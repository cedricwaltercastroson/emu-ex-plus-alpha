#include "EEprom.h"
#include "../Util.h"
#include "GBA.h"
#include <memory.h>
#include <string.h>
#include <imagine/util/algorithm.h>
#include <imagine/logger/logger.h>

int eepromMode = EEPROM_IDLE;
int eepromByte = 0;
int eepromBits = 0;
int eepromAddress = 0;

IG::ByteBuffer eepromData;

uint8_t eepromBuffer[16];
bool eepromInUse = false;
int eepromSize = SIZE_EEPROM_512;

static auto eepromSaveData(uint8_t *eepromData)
{
  return std::array<variable_desc, 8>
  {{
    { &eepromMode, sizeof(int) },
    { &eepromByte, sizeof(int) },
    { &eepromBits, sizeof(int) },
    { &eepromAddress, sizeof(int) },
    { &eepromInUse, sizeof(bool) },
    { &eepromData[0], SIZE_EEPROM_512 },
    { &eepromBuffer[0], 16 },
    { NULL, 0 }
  }};
}

void eepromInit()
{
    eepromInUse = false;
    eepromSize = SIZE_EEPROM_512;
    eepromData = SIZE_EEPROM_8K;
    memset(eepromData.data(), 255, eepromData.size());
}

void eepromReset()
{
    eepromMode = EEPROM_IDLE;
    eepromByte = 0;
    eepromBits = 0;
    eepromAddress = 0;
}

void eepromSaveGame(uint8_t*& data)
{
    uint8_t eepromDataTemp[SIZE_EEPROM_8K]{};
    IG::copy_n(eepromData.data(), eepromData.size(), eepromDataTemp);
    utilWriteDataMem(data, eepromSaveData(eepromDataTemp).data());
    utilWriteIntMem(data, eepromSize);
    utilWriteMem(data, eepromDataTemp, SIZE_EEPROM_8K);
}

void eepromReadGame(const uint8_t*& data)
{
    uint8_t eepromDataTemp[SIZE_EEPROM_8K]{};
    utilReadDataMem(data, eepromSaveData(eepromDataTemp).data());
    eepromSize = utilReadIntMem(data);
    utilReadMem(eepromDataTemp, data, SIZE_EEPROM_8K);
    IG::copy_n(eepromDataTemp, eepromData.size(), eepromData.data());
}


void eepromSaveGame(gzFile gzFile)
{
    uint8_t eepromDataTemp[SIZE_EEPROM_8K]{};
    IG::copy_n(eepromData.data(), eepromData.size(), eepromDataTemp);
    utilWriteData(gzFile, eepromSaveData(eepromDataTemp).data());
    utilWriteInt(gzFile, eepromSize);
    utilGzWrite(gzFile, eepromDataTemp, SIZE_EEPROM_8K);
}

void eepromReadGame(gzFile gzFile, int version)
{
    uint8_t eepromDataTemp[SIZE_EEPROM_8K]{};
    utilReadData(gzFile, eepromSaveData(eepromDataTemp).data());
    // prior to 0.7.1, only 4K EEPROM was supported
    int eepromSizeTemp = SIZE_EEPROM_512;
    if (version >= SAVE_GAME_VERSION_3) {
      eepromSizeTemp = utilReadInt(gzFile);
      utilGzRead(gzFile, eepromDataTemp, SIZE_EEPROM_8K);
    }
    if(eepromSizeTemp != eepromSize)
    {
      logWarn("expected EEPROM size:%d but got %d from state", eepromSize, eepromSizeTemp);
    }
    IG::copy_n(eepromDataTemp, eepromData.size(), eepromData.data());
}

void eepromReadGameSkip(gzFile gzFile, int version)
{
    // skip the eeprom data in a save game
    uint8_t eepromDataTemp[SIZE_EEPROM_8K];
    utilReadDataSkip(gzFile, eepromSaveData(eepromDataTemp).data());
    if (version >= SAVE_GAME_VERSION_3) {
        utilGzSeek(gzFile, sizeof(int), SEEK_CUR);
        utilGzSeek(gzFile, SIZE_EEPROM_8K, SEEK_CUR);
    }
}

int eepromRead(uint32_t /* address */)
{
    switch (eepromMode) {
    case EEPROM_IDLE:
    case EEPROM_READADDRESS:
    case EEPROM_WRITEDATA:
        return 1;
    case EEPROM_READDATA: {
        eepromBits++;
        if (eepromBits == 4) {
            eepromMode = EEPROM_READDATA2;
            eepromBits = 0;
            eepromByte = 0;
        }
        return 0;
    }
    case EEPROM_READDATA2: {
        int address = eepromAddress << 3;
        int mask = 1 << (7 - (eepromBits & 7));
        int data = (eepromData[address + eepromByte] & mask) ? 1 : 0;
        eepromBits++;
        if ((eepromBits & 7) == 0)
            eepromByte++;
        if (eepromBits == 0x40)
            eepromMode = EEPROM_IDLE;
        return data;
    }
    default:
        return 0;
    }
    return 1;
}

void eepromWrite(uint32_t /* address */, uint8_t value, int cpuDmaCount)
{
    if (cpuDmaCount == 0)
        return;
    int bit = value & 1;
    switch (eepromMode) {
    case EEPROM_IDLE:
        eepromByte = 0;
        eepromBits = 1;
        eepromBuffer[eepromByte] = bit;
        eepromMode = EEPROM_READADDRESS;
        break;
    case EEPROM_READADDRESS:
        eepromBuffer[eepromByte] <<= 1;
        eepromBuffer[eepromByte] |= bit;
        eepromBits++;
        if ((eepromBits & 7) == 0) {
            eepromByte++;
        }
        if (cpuDmaCount == 0x11 || cpuDmaCount == 0x51) {
            if (eepromBits == 0x11) {
                eepromInUse = true;
                eepromSize = SIZE_EEPROM_8K;
                eepromAddress = ((eepromBuffer[0] & 0x3F) << 8) | ((eepromBuffer[1] & 0xFF));
                if (!(eepromBuffer[0] & 0x40)) {
                    eepromBuffer[0] = bit;
                    eepromBits = 1;
                    eepromByte = 0;
                    eepromMode = EEPROM_WRITEDATA;
                } else {
                    eepromMode = EEPROM_READDATA;
                    eepromByte = 0;
                    eepromBits = 0;
                }
            }
        } else {
            if (eepromBits == 9) {
                eepromInUse = true;
                eepromAddress = (eepromBuffer[0] & 0x3F);
                if (!(eepromBuffer[0] & 0x40)) {
                    eepromBuffer[0] = bit;
                    eepromBits = 1;
                    eepromByte = 0;
                    eepromMode = EEPROM_WRITEDATA;
                } else {
                    eepromMode = EEPROM_READDATA;
                    eepromByte = 0;
                    eepromBits = 0;
                }
            }
        }
        break;
    case EEPROM_READDATA:
    case EEPROM_READDATA2:
        // should we reset here?
        eepromMode = EEPROM_IDLE;
        break;
    case EEPROM_WRITEDATA:
        eepromBuffer[eepromByte] <<= 1;
        eepromBuffer[eepromByte] |= bit;
        eepromBits++;
        if ((eepromBits & 7) == 0) {
            eepromByte++;
        }
        if (eepromBits == 0x40) {
            eepromInUse = true;
            // write data;
            for (int i = 0; i < 8; i++) {
                eepromData[(eepromAddress << 3) + i] = eepromBuffer[i];
            }
            systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
        } else if (eepromBits == 0x41) {
            eepromMode = EEPROM_IDLE;
            eepromByte = 0;
            eepromBits = 0;
        }
        break;
    }
}
