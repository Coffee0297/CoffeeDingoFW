#include "config.h"
#include "error.h"
#include "crc.h"
#include "param_protocol.h"
#include "hardware/mb85rc.h"
#include "device.h"
#include <cstring>
#include <cstddef>

#if HAS_EXT_MEMORY
MB85RC fram(I2CD1, MB85RC_I2CADDR_DEFAULT);
#endif

bool ReadConfigExt()
{
    #if HAS_EXT_MEMORY
    // Read config
    if(!fram.Read(0x0, (uint8_t*)&stConfig, sizeof(stConfig))) {
        return false;
    }
    
    // Check version number
    if(stConfig.stDevice.nConfigVersion != CONFIG_VERSION) {
        return false;
    }
    
    // Read stored data CRC
    uint32_t storedCrc = 0;
    if(!fram.Read(sizeof(stConfig), (uint8_t*)&storedCrc, sizeof(storedCrc))) {
        return false;
    }
    
    // Calculate CRC of the data we just read
    uint32_t calculatedCrc = CalculateCRC32(&stConfig, sizeof(stConfig), 0xFFFFFFFF);
    
    // Compare CRCs to verify data integrity
    if(storedCrc != calculatedCrc) {
        return false; // Data corrupt or changed
    }
    
    return true;
    #else
    return false;
    #endif
}

bool WriteConfigExt(){
    #if HAS_EXT_MEMORY
    if(!fram.CheckId()) {
        return false;
    }
    
    // Make sure the version is current
    stConfig.stDevice.nConfigVersion = CONFIG_VERSION;
    
    // Write config
    if(!fram.Write(0x0, (uint8_t*)&stConfig, sizeof(stConfig))) {
        return false;
    }
    
    // Calculate config CRC
    uint32_t dataCrc = CalculateCRC32(&stConfig, sizeof(stConfig), 0xFFFFFFFF);
    
    // Write CRC after config
    if(!fram.Write(sizeof(stConfig), (uint8_t*)&dataCrc, sizeof(dataCrc))) {
        return false;
    }
    
    return true;
    #else
    return false;
    #endif
}

void InitConfigExt()
{
    #if HAS_EXT_MEMORY
    if(!fram.CheckId())
        Error::SetFatalError(FatalErrorType::ErrFRAM, MsgSrc::Config);

    if(!ReadConfigExt())
    {
        if(fram.GetErrors() != 0)
            Error::SetFatalError(FatalErrorType::ErrFRAM, MsgSrc::Config);
        
        //Write default for next power cycle
        SetAllDefaultParams();
        if(!WriteConfigExt()){
            //Couldn't write default config
            //FRAM issue 
            Error::SetFatalError(FatalErrorType::ErrFRAM, MsgSrc::Config);
        }
        else
        {
            //Wrote default config
            //Error to force power cycle
            Error::SetFatalError(FatalErrorType::ErrConfig, MsgSrc::Config);
        }
    }
    #endif
}

#if HAS_EXT_MEMORY
//=============================================================================
// Retained values: block state kept over power cycles and sleep (wake = reset)
// Only state-holding blocks can resume; every other signal is recomputed each cycle
//=============================================================================
static const uint16_t RETAIN_ADDR = 0x7800;
static const uint32_t RETAIN_MAGIC = 0x4E544552; // "RETN"
static_assert(sizeof(DeviceConfig) + 4 <= 0x7000, "config overlaps the FRAM regions above it");

struct RetainData{
  uint32_t nMagic;
  uint8_t nCounter[NUM_COUNTERS];
  bool bVirtIn[NUM_VIRT_INPUTS];
  #if NUM_KEYPADS > 0
  bool bButton[NUM_KEYPADS][KEYPAD_MAX_BUTTONS];
  #endif
  uint32_t nCrc;
};
static_assert(sizeof(RetainData) <= 0x400, "retained values exceed their FRAM region");

static RetainData stRetainWritten;

static uint32_t RetainCrc(const RetainData *p)
{
    return CalculateCRC32(p, offsetof(RetainData, nCrc), 0xFFFFFFFF);
}

void RestoreRetained()
{
    RetainData st;
    if (!fram.Read(RETAIN_ADDR, (uint8_t*)&st, sizeof(st)) ||
        (st.nMagic != RETAIN_MAGIC) || (st.nCrc != RetainCrc(&st)))
        return; // blank, torn or from another layout: start from defaults

    stRetainWritten = st;

    for (uint8_t i = 0; i < NUM_COUNTERS; i++)
        if (stConfig.stCounter[i].bRetain)
            counter[i].fVal = st.nCounter[i]; // Update clamps it to the configured range

    for (uint8_t i = 0; i < NUM_VIRT_INPUTS; i++)
        if (stConfig.stVirtualInput[i].bRetain)
            virtIn[i].Restore(st.bVirtIn[i]);

    #if NUM_KEYPADS > 0
    for (uint8_t k = 0; k < NUM_KEYPADS; k++)
        for (uint8_t b = 0; b < KEYPAD_MAX_BUTTONS; b++)
            if (stConfig.stKeypad[k].stButton[b].bRetain)
            {
                keypad[k].button[b].input.Restore(st.bButton[k][b]);
                keypad[k].fButtonVal[b] = st.bButton[k][b];
            }
    #endif
}

// SlowThread (and sleep entry): writes only when a retained value changed
void WriteRetained()
{
    RetainData st = {};
    st.nMagic = RETAIN_MAGIC;

    for (uint8_t i = 0; i < NUM_COUNTERS; i++)
        if (stConfig.stCounter[i].bRetain)
            st.nCounter[i] = counter[i].fVal;

    for (uint8_t i = 0; i < NUM_VIRT_INPUTS; i++)
        if (stConfig.stVirtualInput[i].bRetain)
            st.bVirtIn[i] = virtIn[i].fVal;

    #if NUM_KEYPADS > 0
    for (uint8_t k = 0; k < NUM_KEYPADS; k++)
        for (uint8_t b = 0; b < KEYPAD_MAX_BUTTONS; b++)
            if (stConfig.stKeypad[k].stButton[b].bRetain)
                st.bButton[k][b] = keypad[k].button[b].bVal;
    #endif

    st.nCrc = RetainCrc(&st);

    if ((memcmp(&st, &stRetainWritten, sizeof(st)) != 0) &&
        fram.Write(RETAIN_ADDR, (uint8_t*)&st, sizeof(st)))
        stRetainWritten = st;
}
#endif