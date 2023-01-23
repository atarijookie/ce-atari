#include <string>
#include "utils.h"
#include "global.h"

// These were global const string constants, but now they depend on .env content, so they are now
// loaded on app start and used when needed.
std::string corePath;
std::string CE_CONF_FDD_IMAGE_PATH_AND_FILENAME;
std::string FDD_TEST_IMAGE_PATH_AND_FILENAME;
std::string PATH_CE_DD_BS_L1;
std::string PATH_CE_DD_BS_L2;
std::string PATH_CE_DD_PRG_PATH_AND_FILENAME;

void preloadGlobalsFromDotEnv(void)
{
    corePath                            = Utils::dotEnvValue("CORE_SERVICE_PATH");
    std::string s1(CE_CONF_FDD_IMAGE_JUST_FILENAME);
    CE_CONF_FDD_IMAGE_PATH_AND_FILENAME = Utils::mergeHostPaths2(corePath, s1);
    std::string s2(FDD_TEST_IMAGE_JUST_FILENAME);
    FDD_TEST_IMAGE_PATH_AND_FILENAME    = Utils::mergeHostPaths2(corePath, s2);
    std::string s3("configdrive/drivers/ce_dd.bs");
    PATH_CE_DD_BS_L1                    = Utils::mergeHostPaths2(corePath, s3);
    std::string s4("configdrive/drivers/ce_dd_l2.bs");
    PATH_CE_DD_BS_L2                    = Utils::mergeHostPaths2(corePath, s4);
    std::string s5("configdrive/drivers/ce_dd.prg");
    PATH_CE_DD_PRG_PATH_AND_FILENAME    = Utils::mergeHostPaths2(corePath, s5);
}
