// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

#include "floppyimagefactory.h"
#include "floppyimagest.h"
#include "floppyimagemsa.h"
#include "../debug.h"
#include "../utils.h"

FloppyImage *FloppyImageFactory::getImage(const char *fileName)
{
    FloppyImage *img = NULL;
    const char *ext = Utils::getExtension(fileName);    // find extensions

    if(ext == NULL) {                       // extension not found? fail
        return NULL;
    }

    //--------------
    // if it's a ZIP file, chek if it contains an supported floppy image
    std::string fileNameInZip;

    if(strcasecmp(ext, "zip") == 0) {       // if it's a ZIP file
        Debug::out(LOG_DEBUG, "FloppyImageFactory -- file %s is a ZIP file, will search for supported image inside", fileName);

                                            // decompress the ZIP file and search for floppy image inside, return path to first valid image
        bool foundValidImage = Utils::unZIPfloppyImageAndReturnFirstImage(fileName, fileNameInZip);

        if(!foundValidImage) {              // image not found in ZIP file, fail
            Debug::out(LOG_DEBUG, "FloppyImageFactory -- ZIP file %s doesn't contain supported image inside", fileName);
            return NULL;
        }

        Debug::out(LOG_DEBUG, "FloppyImageFactory -- ZIP file %s contains image: %s", fileName, fileNameInZip.c_str());

        fileName = fileNameInZip.c_str();    // now the filename contains path to image extracted from ZIP file
        ext      = Utils::getExtension(fileName);   // find extension

        if(ext == NULL) {                       // extension not found? fail
            return NULL;
        }
    }

    //--------------
    if(strcasecmp(ext, "msa") == 0) {   // msa image?
        Debug::out(LOG_DEBUG, "FloppyImageFactory -- using MSA image on %s", fileName);
        img = new FloppyImageMsa();

    } else if(strcasecmp(ext, "st") == 0) {    // st image?
        Debug::out(LOG_DEBUG, "FloppyImageFactory -- using ST image on %s", fileName);
        img = new FloppyImageSt();
    } else {
        Debug::out(LOG_DEBUG, "FloppyImageFactory -- Image file %s type %s not supported", fileName, ext);
    }

    if(img) {
        img->open(fileName);            // open the new image
    }
    return img;                        // unknown extension?
}


