#include <stdio.h>
#include <string>
#include "utils.h"
#include "global.h"
#include "debug.h"

// These were global const string constants, but now they depend on .env content, so they are now
// loaded on app start and used when needed.
std::string corePath;
std::string CE_CONF_FDD_IMAGE_PATH_AND_FILENAME;
std::string FDD_TEST_IMAGE_PATH_AND_FILENAME;
std::string PATH_CE_DD_BS_L1;
std::string PATH_CE_DD_BS_L2;
std::string PATH_CE_DD_PRG_PATH_AND_FILENAME;
std::string CONFIG_DRIVE_PATH;

void preloadGlobalsFromDotEnv(void)
{
    corePath = Utils::dotEnvValue("BIN_DIR");     // path to where the core service is stored

    CE_CONF_FDD_IMAGE_PATH_AND_FILENAME = Utils::mergeHostPaths3(corePath, CE_CONF_FDD_IMAGE_JUST_FILENAME);
    FDD_TEST_IMAGE_PATH_AND_FILENAME = Utils::mergeHostPaths3(corePath, FDD_TEST_IMAGE_JUST_FILENAME);

    // path to where the drivers inside the core service are stored
    std::string driversPath = Utils::mergeHostPaths3(corePath, "configdrive/drivers");

    // files loaded on start - take them from .env, fall back to drivers path if not specified there
    PATH_CE_DD_BS_L1 = Utils::dotEnvValue("CE_DD_BS_L1_FILE", Utils::mergeHostPaths3(driversPath, "ce_dd.bs").c_str());
    PATH_CE_DD_BS_L2 = Utils::dotEnvValue("CE_DD_BS_L2_FILE", Utils::mergeHostPaths3(driversPath, "ce_dd_l2.bs").c_str());
    PATH_CE_DD_PRG_PATH_AND_FILENAME = Utils::dotEnvValue("CE_DD_PRG_FILE", Utils::mergeHostPaths3(driversPath, "ce_dd.prg").c_str());

    CONFIG_DRIVE_PATH = Utils::dotEnvValue("CONFIG_DRIVE_PATH");     // where the copy of configdrive is

    logHdd(LOG_DEBUG, "CORE_PATH        : %s", corePath.c_str());
    logHdd(LOG_DEBUG, "driversPath      : %s", driversPath.c_str());
    logHdd(LOG_DEBUG, "CONFIG_DRIVE_PATH: %s", CONFIG_DRIVE_PATH.c_str());
    logHdd(LOG_DEBUG, "CE_DD_BS_L1_FILE : %s", PATH_CE_DD_BS_L1.c_str());
    logHdd(LOG_DEBUG, "CE_DD_BS_L2_FILE : %s", PATH_CE_DD_BS_L2.c_str());
    logHdd(LOG_DEBUG, "CE_DD_PRG_FILE   : %s", PATH_CE_DD_PRG_PATH_AND_FILENAME.c_str());
}

// check that all the files loaded on start and all the dirs from .env exist, return false if something is missing
bool checkStartupFilesAndDirs(void)
{
    bool allGood = true;

    // files loaded by ce_cores at start
    std::string logoPath = Utils::dotEnvValue("CE_LOGO_FILE", "ce_logo.bin");
    const char* files[][2] = {
        {"CE_DD_BS_L1_FILE", PATH_CE_DD_BS_L1.c_str()},
        {"CE_DD_BS_L2_FILE", PATH_CE_DD_BS_L2.c_str()},
        {"CE_DD_PRG_FILE",   PATH_CE_DD_PRG_PATH_AND_FILENAME.c_str()},
        {"CE_LOGO_FILE",     logoPath.c_str()}
    };

    for(size_t i=0; i<sizeof(files)/sizeof(files[0]); i++) {
        FILE *f = fopen(files[i][1], "rb");     // try to open the file for reading

        if(!f) {
            logHdd(LOG_ERROR, "Startup check - %s - cannot open file '%s'", files[i][0], files[i][1]);
            printf("Startup check - %s - cannot open file '%s'\n", files[i][0], files[i][1]);
            allGood = false;
            continue;
        }

        fclose(f);
    }

    // dirs specified in .env
    const char* dirKeys[] = {"CE_DIR", "BIN_DIR", "DATA_DIR", "LOG_DIR", "PID_DIR", "SETTINGS_DIR", "CONFIG_DRIVE_PATH", "IKBD_VIRTUAL_DEVICES_PATH"};

    for(size_t i=0; i<sizeof(dirKeys)/sizeof(dirKeys[0]); i++) {
        std::string dir = Utils::dotEnvValue(dirKeys[i]);

        if(dir.empty() || !Utils::dirExists(dir)) {
            logHdd(LOG_ERROR, "Startup check - %s - directory '%s' does not exist", dirKeys[i], dir.c_str());
            printf("Startup check - %s - directory '%s' does not exist\n", dirKeys[i], dir.c_str());
            allGood = false;
        }
    }

    return allGood;
}
