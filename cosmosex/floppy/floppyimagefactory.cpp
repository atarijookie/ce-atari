#include <string.h>
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
    int len = strlen(fileName);
    int pos = -1;

    for(int i=(len-1); i>=0; i--) {     // find last '.'
        if(fileName[i] == '.') {
            pos = i;
            break;
        }
    }

    if(pos == -1) {                     // last '.' not found? fail
        return NULL;
    }

    char ext[3];
    toLowerCase(&fileName[pos+1], ext); // convert the extension to lower case

    if(strncmp(ext, "msa", 3) == 0) {   // msa image?
        Debug::out("FloppyImageFactory -- using MSA image on %s", fileName);

        if(!msa) {                      // not created yet?
            msa = new FloppyImageMsa();
        }

        msa->close();
        msa->open(fileName);            // open the new image

        return msa;                     // and return the pointer
    }

    if(strncmp(ext, "st", 2) == 0) {    // st image?
        Debug::out("FloppyImageFactory -- using ST image on %s", fileName);

        if(!st) {                       // not created yet?
            st = new FloppyImageSt();
        }

        st->close();
        st->open(fileName);             // open the new image

        return st;                      // and return the pointer
    }

    return NULL;                        // unknown extension?
}

void FloppyImageFactory::toLowerCase(char *orig, char *lower)
{
    for(int i=0; i<3; i++) {
        lower[i] = lowerCase(orig[i]);
    }
}

char FloppyImageFactory::lowerCase(char in)
{
    if(in >= 65 && in <= 90) {      // if it's upper case, convert it to lower case
        in = in + 32;
    }

    return in;
}

