#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>

#include "global.h"
#include "debug.h"
#include "settings.h"
#include "update.h"
#include "utils.h"

Versions Update::versions;

extern THwConfig hwConfig;

void Update::initialize(void)
{
    char appVersion[16];
    Version::getAppVersion(appVersion);

    Update::versions.app.fromString(                (char *) appVersion);
    Update::versions.xilinx.fromFirstLineOfFile(    (char *) XILINX_VERSION_FILE, false);   // xilinx version file without dashes
}

