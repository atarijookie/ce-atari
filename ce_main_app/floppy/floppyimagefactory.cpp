#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
 
#include "floppyimagefactory.h"
#include "../debug.h"

FloppyImageFactory::FloppyImageFactory()
{
    msa = NULL;
    st  = NULL;
}

FloppyImageFactory::~FloppyImageFactory()
{
    if(msa) {
        msa->close();
        delete msa;
    }

    if(st) {
        st->close();
        delete st;
    }
}

IFloppyImage *FloppyImageFactory::getImage(char *fileName)
{
    char *ext = strrchr(fileName, '.');     // find last '.'

    if(ext == NULL) {                       // last '.' not found? fail
        return NULL;
    }

    ext++;                                  // move beyond '.'
    
    //--------------
    // if it's a ZIP file, chek if it contains an supported floppy image
    char fileNameInZip[256];
    memset(fileNameInZip, 0, sizeof(fileNameInZip));

    if(strcasecmp(ext, "zip") == 0) {       // if it's a ZIP file
        Debug::out(LOG_DEBUG, "FloppyImageFactory -- file %s is a ZIP file, will search for supported image inside", fileName);
        
                                            // decompress the ZIP file and search for floppy image inside, return path to first valid image
        bool foundValidImage = handleZIPedImage(fileName, fileNameInZip);
        
        if(!foundValidImage) {              // image not found in ZIP file, fail
            Debug::out(LOG_DEBUG, "FloppyImageFactory -- ZIP file %s doesn't contain supported image inside", fileName);
            return NULL;
        }

        Debug::out(LOG_DEBUG, "FloppyImageFactory -- ZIP file %s contains image: %s", fileName, fileNameInZip);
        
        fileName    = fileNameInZip;            // now the filename contains path to image extracted from ZIP file
        ext         = strrchr(fileName, '.');   // find last '.'

        if(ext == NULL) {                       // last '.' not found? fail
            return NULL;
        }
        
        ext++;
    }    
    
    //--------------
    if(strcasecmp(ext, "msa") == 0) {   // msa image?
        Debug::out(LOG_DEBUG, "FloppyImageFactory -- using MSA image on %s", fileName);

        if(!msa) {                      // not created yet?
            msa = new FloppyImageMsa();
        }

        msa->close();
        msa->open(fileName);            // open the new image

        return msa;                     // and return the pointer
    }

    if(strcasecmp(ext, "st") == 0) {    // st image?
        Debug::out(LOG_DEBUG, "FloppyImageFactory -- using ST image on %s", fileName);

        if(!st) {                       // not created yet?
            st = new FloppyImageSt();
        }

        st->close();
        st->open(fileName);             // open the new image

        return st;                      // and return the pointer
    }

    return NULL;                        // unknown extension?
}

bool FloppyImageFactory::handleZIPedImage(const char *inZipFilePath, char *outImageFilePath)
{
    outImageFilePath[0] = 0;                        // out path doesn't contain anything yet
    
    system("rm    -rf /tmp/zipedfloppy");           // delete this dir, if it exists
    system("mkdir -p  /tmp/zipedfloppy");           // create that dir
    
    char unzipCommand[512];
    sprintf(unzipCommand, "unzip -o %s -d /tmp/zipedfloppy > /dev/null 2> /dev/null", inZipFilePath);
    system(unzipCommand);                           // unzip the downloaded ZIP file into that tmp directory
    
    // find the first usable floppy image
    DIR *dir = opendir("/tmp/zipedfloppy");         // try to open the dir
    
    if(dir == NULL) {                               // not found?
        Debug::out(LOG_DEBUG, "FloppyImageFactory::handleZIPedImage -- opendir() failed");
        return false;
    }

    bool found          = false;
    struct dirent *de   = NULL;
    
    char *pExt = NULL;
    
    while(1) {                                      // avoid buffer overflow
        de = readdir(dir);                          // read the next directory entry
    
        if(de == NULL) {                            // no more entries?
            break;
        }

        if(de->d_type != DT_REG) {                  // not a file? skip it
            continue;
        }

        int fileNameLen = strlen(de->d_name);       // get length of filename

        if(fileNameLen < 3) {                       // if it's too short, skip it
            continue;
        }

        pExt = strrchr(de->d_name, '.');            // find last '.'

        if(pExt == NULL) {                          // last '.' not found? skip it
            continue;
        }
        pExt++;                                     // move beyond '.'
        
        if(strcasecmp(pExt, "st") == 0 || strcasecmp(pExt, "msa") == 0) {  // the extension of the file is valid for a floppy image? 
            found = true;
            break;
        }
    }

    closedir(dir);                                  // close the dir
    
    if(!found) {                                    // not found? return with a fail
        return false;
    }
    
    // construct path to unZIPed image
    strcpy(outImageFilePath, "/tmp/zipedfloppy/");
    strcat(outImageFilePath, de->d_name);
    
    return true;
}

