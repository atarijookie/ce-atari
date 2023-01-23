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
    corePath = Utils::dotEnvValue("CORE_SERVICE_PATH");     // path to where the core service is stored

    CE_CONF_FDD_IMAGE_PATH_AND_FILENAME = Utils::mergeHostPaths3(corePath, CE_CONF_FDD_IMAGE_JUST_FILENAME);
    FDD_TEST_IMAGE_PATH_AND_FILENAME = Utils::mergeHostPaths3(corePath, FDD_TEST_IMAGE_JUST_FILENAME);

    // path to where the drivers inside the core service are stored
    std::string driversPath = Utils::mergeHostPaths3(corePath, "configdrive/drivers");

    PATH_CE_DD_BS_L1 = Utils::mergeHostPaths3(driversPath, "ce_dd.bs");
    PATH_CE_DD_BS_L2 = Utils::mergeHostPaths3(driversPath, "ce_dd_l2.bs");
    PATH_CE_DD_PRG_PATH_AND_FILENAME = Utils::mergeHostPaths3(driversPath, "ce_dd.prg");

    CONFIG_DRIVE_PATH = Utils::dotEnvValue("CONFIG_PATH_COPY");     // where the copy of configdrive is

    Debug::out(LOG_DEBUG, "CORE_SERVICE_PATH: %s", corePath.c_str());
    Debug::out(LOG_DEBUG, "driversPath      : %s", driversPath.c_str());
    Debug::out(LOG_DEBUG, "CONFIG_DRIVE_PATH: %s", CONFIG_DRIVE_PATH.c_str());
}
