#ifndef _SCREENCASTSERVICE_H_
#define _SCREENCASTSERVICE_H_

#include <string>

class ScreencastService
{
public:
    ScreencastService();
    void start();
	void stop();
    int getFrameSkip();
	void setSTResolution(unsigned char iSTResolution);
	unsigned char getSTResolution();
    void setPalette(void* pxPalette);
    void getPalette(void* pxPalette);
    void setScreen(void* pxScreen);
    void getScreen(void* pxScreen);
private:
    unsigned char *pxScreen;
    unsigned char *pxPalette;
    unsigned char iSTResolution;
};
#endif