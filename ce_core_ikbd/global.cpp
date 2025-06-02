#include <string>
#include "utils.h"
#include "global.h"
#include "debug.h"

// These were global const string constants, but now they depend on .env content, so they are now
// loaded on app start and used when needed.
std::string corePath;

extern TFlags flags;

void preloadGlobalsFromDotEnv(void)
{
    corePath = Utils::dotEnvValue("BIN_DIR");     // path to where the core service is stored
}
