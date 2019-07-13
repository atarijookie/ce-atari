// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#include <signal.h>
#include <iostream>
#include "../floppy/imagesilo.h"
#include "../floppy/floppyencoder.h"
#include "../utils.h"

volatile sig_atomic_t sigintReceived = 0;
TFlags flags;

// returns 0 for success
int test(const char * filename)
{
    int ret = 0;
    int tracks, sides, sectorsPerTrack;
    ImageSilo* pxImageSilo = new ImageSilo();
    pxImageSilo->setCurrentSlot(0);
    int iSlot = pxImageSilo->getCurrentSlot();
    std::cout << "Current slot : " << iSlot << std::endl;
    std::string sEmpty;
    std::string sLocalFileWPath(filename);
    std::string sFile(filename);
    //std::cout << "floppyEncodingRunning " << pxImageSilo->getFloppyEncodingRunning() << std::endl;
    std::cout << "containsImage(" << sFile << ")=" << pxImageSilo->containsImage(sFile.c_str()) << std::endl;
    pxImageSilo->add(iSlot, sFile, sLocalFileWPath, sEmpty, true);
    Utils::sleepMs(500);
    std::cout << "containsImage(" << sFile << ")=" << pxImageSilo->containsImage(sFile.c_str()) << std::endl;
    //std::cout << "floppyEncodingRunning " << pxImageSilo->getFloppyEncodingRunning() << std::endl;
    if (pxImageSilo->getParams(tracks, sides, sectorsPerTrack))
        std::cout << sides << " sides, " << tracks << " tracks of " << sectorsPerTrack << " sectors." << std::endl;
    else
        std::cout << "*** Failed to getParams() ***" << std::endl;
    Utils::sleepMs(500);
    std::cout << "containsImage(" << sFile << ")=" << pxImageSilo->containsImage(sFile.c_str()) << std::endl;
    if (pxImageSilo->getParams(tracks, sides, sectorsPerTrack)) {
        int track, side;
        std::cout << sides << " sides, " << tracks << " tracks of " << sectorsPerTrack << " sectors." << std::endl;
        if (tracks == 0 || sides == 0)
            ret = 3;
        for (track = 0; track < tracks; track++) {
            for (side = 0; side < sides; side++) {
                int trackdatalen;
                BYTE * trackdata = pxImageSilo->getEncodedTrack(track, side, trackdatalen);
                if (trackdata != NULL) {
                    std::cout << "getEncodedTrack(" << track << ", " << side << ") : " << trackdatalen << " bytes" << std::endl;
                    char tmp[256];
                    snprintf(tmp, sizeof(tmp), "track%02d_s%d.mfm", track, side);
                    FILE * f = fopen(tmp, "wb");
                    fwrite(trackdata, 1, trackdatalen, f);
                    fclose(f);
                } else {
                    std::cout << "*** getEncodedTrack(" << track << ", " << side << ") failed ***" << std::endl;
                    ret = 2;
                }
            }
        }
    } else {
        std::cout << "*** Failed to getParams() ***" << std::endl;
        ret = 1;
    }
    delete pxImageSilo;
    return ret;
}

int main(int argc, char * * argv)
{
    int r;
    const char * image_filename = "/tmp/emptyimage.st";
    pthread_t  floppyEncThreadInfo;

    if (argc > 1)
        image_filename = argv[1];
    std::cout << "testing floppy code" << std::endl;
    if (pthread_create(&floppyEncThreadInfo, NULL, floppyEncodeThreadCode, NULL) != 0)
        std::cout << "pthread_create error" << std::endl;
    r = test(image_filename);
    printf("Stoping floppy encoder thread\n");
    floppyEncoder_stop();
    pthread_join(floppyEncThreadInfo, NULL);            // wait until floppy encode thread finishes

    return r;
}
