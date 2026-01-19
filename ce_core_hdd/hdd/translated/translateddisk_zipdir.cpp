// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <algorithm>
#include <string>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#include "../../../libdospath/libdospath.h"
#include "../../misc/global.h"
#include "../../misc/debug.h"
#include "../../misc/settings.h"
#include "../../misc/utils.h"
#include "../acsidatatrans.h"
#include "../acsicommand/screencastacsicommand.h"
#include "../acsicommand/dateacsicommand.h"
#include "translateddisk.h"
#include "translatedhelper.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "desktopcreator.h"

void TranslatedDisk::createFullHostPath(const std::string &inFullAtariPath, int inAtariDriveIndex, std::string &outFullHostPath)
{
    /*
    This method will take in short (Atari) path, will convert it to host path (where the Atari path is mapped), then will use the
    libDOSpath to convert that short path to long path. If mounting of archives (e.g. ZIP files) is not enabled, the code ends at that point.
    */

    std::string root = conf[inAtariDriveIndex].hostRootPath;    // get root path

    std::vector<std::string> symlinksApplied;       // this will be non-empty if ldp_shortToLongPath() will apply symlink to path
    std::string shortPath = root;
    Utils::mergeHostPaths(shortPath, inFullAtariPath);                      // short path = root + full atari path
    ldp_shortToLongPath(shortPath, outFullHostPath, true, &symlinksApplied); // short path to long path

    logHdd(LOG_DEBUG, "TranslatedDisk::createFullHostPath - shortPath: %s -> outFullHostPath: %s , applied symlinks count: %d", shortPath.c_str(), outFullHostPath.c_str(), symlinksApplied.size());
}
