#include "config.h"
#include "error.h"
#include "crc.h"
#include "param_protocol.h"
#include "config_ext.h"

//Try to read config from internal flash first
//If it fails, try external memory
//If that fails, return false
bool ReadConfig(){
    DeviceConfig tempConfig;

    flash_error_t err = flashRead(CONFIG_FLASH, CONFIG_FLASH_OFFSET,
                                  sizeof(DeviceConfig), (uint8_t*)&tempConfig);
    if (err != FLASH_NO_ERROR)
    {
        return ReadConfigExt();
    }

    if (tempConfig.stDevConfig.nConfigVersion != CONFIG_VERSION)
    {
        return ReadConfigExt();
    }

    uint32_t storedCrc;
    err = flashRead(CONFIG_FLASH, CONFIG_FLASH_OFFSET + sizeof(DeviceConfig),
                    sizeof(uint32_t), (uint8_t*)&storedCrc);
    if (err != FLASH_NO_ERROR)
    {
        return ReadConfigExt();
    }

    if (CalculateCRC32(&tempConfig, sizeof(DeviceConfig)) != storedCrc)
    {
        return ReadConfigExt();
    }

    stConfig = tempConfig;
    return true;
}

//Try to write config from internal flash first
//If it fails, try external memory
//If that fails, return false
bool WriteConfig(){
    stConfig.stDevConfig.nConfigVersion = CONFIG_VERSION;

    flash_error_t err = flashStartEraseSector(CONFIG_FLASH, CONFIG_SECTOR);
    if (err != FLASH_NO_ERROR)
    {
        return WriteConfigExt();
    }

    err = flashWaitErase(CONFIG_FLASH);
    if (err != FLASH_NO_ERROR)
    {
        return WriteConfigExt();
    }

    err = flashProgram(CONFIG_FLASH, CONFIG_FLASH_OFFSET,
                       sizeof(DeviceConfig), (const uint8_t*)&stConfig);
    if (err != FLASH_NO_ERROR)
    {
        return WriteConfigExt();
    }

    uint32_t crc = CalculateCRC32(&stConfig, sizeof(DeviceConfig));
    err = flashProgram(CONFIG_FLASH, CONFIG_FLASH_OFFSET + sizeof(DeviceConfig),
                       sizeof(uint32_t), (const uint8_t*)&crc);

    return (err == FLASH_NO_ERROR);
}

void InitConfig()
{
    eflStart(&EFLD1, NULL);

    if (!ReadConfig())
    {
        SetAllDefaultParams();
        if (!WriteConfig())
            Error::SetFatalError(FatalErrorType::ErrConfig, MsgSrc::Config);
    }

}