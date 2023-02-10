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

#include "../global.h"
#include "../debug.h"
#include "../settings.h"
#include "../utils.h"
#include "acsidatatrans.h"
#include "acsicommand/screencastacsicommand.h"
#include "acsicommand/dateacsicommand.h"
#include "settingsreloadproxy.h"
#include "translateddisk.h"
#include "translatedhelper.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "desktopcreator.h"
#include "../display/displaythread.h"
#include "../../libdospath/libdospath.h"

void TranslatedDisk::fillSupportedArchiveExtensionsIfNeeded(void)
{
    /* Method reads all the supported archive extensions we can mount from .env MOUNT_ARCHIVES_SUPPORTED,
       splits that single comma separated string into vector, so we don't have to split it again every time
       we want to check those extensions. Next time when we already got this vector pre-filled, just do nothing. */

    if(supportedArchiveExtensions.size() > 0) {       // if already got some value here, no need to fill it again
        return;
    }

    std::string extensions = Utils::dotEnvValue("MOUNT_ARCHIVES_SUPPORTED");    // get supported extensions from .env
    Utils::splitString(extensions, ',', supportedArchiveExtensions);    // split coma separated string into std::vector

    if(supportedArchiveExtensions.size() == 0) {        // still nothing in this vector? just add .zip as default
         supportedArchiveExtensions.push_back(".zip");
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::fillSupportedArchiveExtensionsIfNeeded - filled in these supported extensions:");
    for (auto it = begin(supportedArchiveExtensions); it != end(supportedArchiveExtensions); ++it) {
        const char* oneExt = it->c_str();
        Debug::out(LOG_DEBUG, "TranslatedDisk::fillSupportedArchiveExtensionsIfNeeded - %s", oneExt);
    }
}

bool TranslatedDisk::hasArchiveExtension(const std::string& longPath)
{
    /* Go through the vector of supported archive extensions and check if the supplied longPath ends with one
       of those supported extensions, returns true if it does. */

    std::string longPathCopy = longPath;                        // create a copy of the input long path
    std::transform(longPathCopy.begin(), longPathCopy.end(), longPathCopy.begin(), ::tolower);  // path copy to lowercase

    fillSupportedArchiveExtensionsIfNeeded();                   // fill supported archive extensions if needed

    for (auto it = begin(supportedArchiveExtensions); it != end(supportedArchiveExtensions); ++it) {
        const char* oneExt = it->c_str();

        if(Utils::endsWith(longPathCopy, oneExt)) {                 // extension found at end?
            Debug::out(LOG_DEBUG, "TranslatedDisk::hasArchiveExtension - file %s has supported archive extension %s, it's an archive", longPath.c_str(), oneExt);
            return true;        // supported archive was found
        }
    }

    //Debug::out(LOG_DEBUG, "TranslatedDisk::hasArchiveExtension - file %s doesn't have any supported archive extension", longPath.c_str());
    return false;               // supported archive not found
}

void TranslatedDisk::createFullHostPath(const std::string &inFullAtariPath, int inAtariDriveIndex, std::string &outFullHostPath,
    bool &waitingForMount, bool* isInArchive)
{
    /*
    This method will take in short (Atari) path, will convert it to host path (where the Atari path is mapped), then will use the
    libDOSpath to convert that short path to long path. If mounting of archives (e.g. ZIP files) is not enabled, the code ends at that point.

    With mounting of archives enabled it will check if we got this archive file already mounted - if we do, we will return the current
    status of waiting for a mount in waitingForMount variable, and if we don't have this archive mounted, then we will
    send a request to mount this archive to the mounter component.

    The mounter component will tell us later when the mount finishes and we will then flip the flag in archiveMounted from false to true.
    */

    // If pointer to isInArchive is provided, set default value to false.
    // Set to true when checkPathForArchiveAndRequestMountIfNeeded() just found out that this is an archive
    if(isInArchive) {
        *isInArchive = false;
    }

    waitingForMount = false;    // start by assuming that we're not waiting for a mount

    std::string root = conf[inAtariDriveIndex].hostRootPath;    // get root path

    std::vector<std::string> symlinksApplied;       // this will be non-empty if ldp_shortToLongPath() will apply symlink to path
    std::string shortPath = root;
    Utils::mergeHostPaths(shortPath, inFullAtariPath);                      // short path = root + full atari path
    ldp_shortToLongPath(shortPath, outFullHostPath, true, &symlinksApplied); // short path to long path

    if(isInArchive) {           // if pointer to isInArchive is provided, store in it if symlink was applied
        *isInArchive = symlinksApplied.size() > 0;
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::createFullHostPath - shortPath: %s -> outFullHostPath: %s , applied symlinks count: %d", shortPath.c_str(), outFullHostPath.c_str(), symlinksApplied.size());

    if(useZipdirNotFile) {     // mounting of archives enabled? check if this is in archive and waiting for mount
        checkPathForArchiveAndRequestMountIfNeeded(outFullHostPath, waitingForMount, &symlinksApplied, isInArchive);
    } else {                    // mounting of archives not enabled
        Debug::out(LOG_DEBUG, "TranslatedDisk::createFullHostPath - useZipdirNotFile: %d so skipping rest of check", (int) useZipdirNotFile);
    }
}

void TranslatedDisk::isArchiveInThisPath(const std::string& outFullHostPath, bool& pathHasArchive, std::string& zipFilePath)
{
    // This function might be called with a filename directly - /var/run/ce/trans/N/ce_update.zip
    // or it might be called with a search string appended    - /var/run/ce/trans/N/ce_update.zip/ *.*

    pathHasArchive = false;

    std::string justPath, justFilename;
    Utils::splitFilenameFromPath(outFullHostPath, justPath, justFilename);

    if(Utils::fileExists(outFullHostPath) && hasArchiveExtension(outFullHostPath)) {    // if file with full path exists and has archive extension
        pathHasArchive = true;
        zipFilePath = outFullHostPath;
        Debug::out(LOG_DEBUG, "TranslatedDisk::isArchiveInThisPath - archive was found in FULL path: %s", zipFilePath.c_str());
    }

    if(Utils::fileExists(justPath) && hasArchiveExtension(justPath)) {  // if file with part of the path exists and has archive extension
        pathHasArchive = true;
        zipFilePath = justPath;
        Debug::out(LOG_DEBUG, "TranslatedDisk::isArchiveInThisPath - archive was found in PART of path: %s", zipFilePath.c_str());
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::isArchiveInThisPath - outFullHostPath: %s - pathHasArchive: %d", outFullHostPath.c_str(), (int) pathHasArchive);
}

void TranslatedDisk::checkPathForArchiveAndRequestMountIfNeeded(const std::string& outFullHostPath, bool &waitingForMount, std::vector<std::string>* pSymlinksApplied, bool* isInArchive)
{
    /* Function cheks if the supplied outFullHostPath has supported archive in it, and if it does, it will check if
       we're waiting for a mounting of the archive here or not. If pointer to isInArchive is supplied, it will set that
       flag to true if the path is in archive - that will be used in the calling function to NOT show any nested archives
       inside of another archive as folders (no archive folders nesting, only one level in the archive allowed).

       This function might be called with a filename directly - /var/run/ce/trans/N/ce_update.zip
         - if trying to set the path to this dir...
       or it might be called with a search string appended    - /var/run/ce/trans/N/ce_update.zip/ *.*
         - when doing fsFirst() on the content of the archive-as-folder.
    */

    waitingForMount = false;

    std::string zipFilePath;
    bool pathHasArchive = false;

    bool symlinksWereApplied = pSymlinksApplied->size() > 0;       // check if some symlinks were applied to path, it's an archive if they were applied to path
    if(symlinksWereApplied) {       // if symlinks were applied, we have to check all the symlinks for mount
        Debug::out(LOG_DEBUG, "TranslatedDisk::checkPathForArchiveAndRequestMountIfNeeded - symlinks were applied, checking them individually");

        if(isInArchive) {           // if pointer to isInArchive is provided, set to true - we're in archive
            *isInArchive = true;
        }

        // go through all the applied symlinks and check if some of them is still mounting
        for (auto it = begin(*pSymlinksApplied); it != end(*pSymlinksApplied); ++it) {
            bool waitingForMountOne;
            getWaitingForMountAndRequestMountIfNeeded(*it, waitingForMountOne);     // check this symlink
            waitingForMount = waitingForMount || waitingForMountOne;     // if at least one symlink is waiting for mount, we're all waiting for mount
        }
    } else {                        // if no symlinks were applied, just check this path for archive in it
        Debug::out(LOG_DEBUG, "TranslatedDisk::checkPathForArchiveAndRequestMountIfNeeded - symlinks were not applied, checking just path for archive ext");

        isArchiveInThisPath(outFullHostPath, pathHasArchive, zipFilePath);

        if(pathHasArchive) {        // this path has archive in it
            if(isInArchive) {       // if pointer to isInArchive is provided, set to true - we're in archive
                *isInArchive = true;
            }

            getWaitingForMountAndRequestMountIfNeeded(zipFilePath, waitingForMount);    // check if we're waiting for mount
        }
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::checkPathForArchiveAndRequestMountIfNeeded - outFullHostPath: %s - waitingForMount: %d", outFullHostPath.c_str(), (int) waitingForMount);
}

void TranslatedDisk::getWaitingForMountAndRequestMountIfNeeded(const std::string& zipFilePath, bool& waitingForMount)
{
    /* This method will check if we already requested mount of zipFilePath, and if we did, then return status if we're
    waiting for mount of this archive. If we didn't request mount of this zipFilePath, do that now and mark that we
    just did ask for this mount. */

    waitingForMount = false;

    // if archiveMounted contains this path, we're sent a request to mount it already
    bool requestedMountAlready = archiveMounted.find(zipFilePath) != archiveMounted.end();

    if(requestedMountAlready) {             // if we already asked for mounting this archive
        waitingForMount = !archiveMounted[zipFilePath];     // if not mounted yet, then still waiting for mount
        Debug::out(LOG_DEBUG, "TranslatedDisk::getWaitingForMountAndRequestMountIfNeeded - already have requested mount of %s, waitingForMount: %s", zipFilePath.c_str(), waitingForMount ? "true" : "false");
        return;
    }

    if(!Utils::fileExists(zipFilePath)) {   // check if this file really exists
        Debug::out(LOG_WARNING, "TranslatedDisk::getWaitingForMountAndRequestMountIfNeeded - archive: %s does not exist, not trying to mount!", zipFilePath.c_str());
        return;
    }

    // if not requested mount of this archive yet, we need to mount this archive
    Debug::out(LOG_DEBUG, "TranslatedDisk::getWaitingForMountAndRequestMountIfNeeded - will now request mounting of %s archive", zipFilePath.c_str());
    archiveMounted[zipFilePath] = false;            // this archive is not mounted yet
    waitingForMount = true;                         // we're definitelly waiting for mount now

    // send command to mounter to mount this archive
    std::string jsonString = "{\"cmd_name\": \"mount_zip\", \"path\": \"";
    jsonString += zipFilePath;                      // add path to archive file we should now mount
    jsonString += "\"}";
    Utils::sendToMounter(jsonString);               // send to socket
}

void TranslatedDisk::handleZipMounted(std::string& zip_path, std::string& mount_path)
{
    // Call this method when some (ZIP) archive with zip_path got mounted to mount_path.
    // Empty mount_path means unmounted, non-empty mount_path means mounted.
    ldp_symlink(zip_path, mount_path);  // add or remove symlink from zip_path (remove if mount_path is empty)

    if(mount_path.empty()) {        // Empty mount_path means unmounted
        if(archiveMounted.find(zip_path) != archiveMounted.end()) {     // we got this path in our map? erase it now
            archiveMounted.erase(zip_path);
        }
    } else {                        // non-empty mount_path means mounted
        archiveMounted[zip_path] = true;        // we're mounted now
    }
}
