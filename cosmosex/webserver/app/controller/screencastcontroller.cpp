#include "screencastcontroller.h"
#include <string>
#include <sstream>
#include <fstream>
#include <streambuf>
#include <string.h>
#include "version.h"

#include "service/screencastservice.h"

ScreencastController::ScreencastController(ScreencastService* pxScreencastService):pxScreencastService(pxScreencastService)
{
    pxPalette=new unsigned char[16*2];
    pxScreen=new unsigned char[32000];
    memset(pxPalette,0,32);
    memset(pxScreen,0,32000);
}

ScreencastController::~ScreencastController() 
{
    delete pxScreen;
    delete pxPalette;
    pxScreen=NULL;
    pxPalette=NULL;
}

bool ScreencastController::getscreenAction(mg_connection *conn, mg_request_info *req_info) 
{
    mg_printf(conn, "HTTP/1.1 200 OK\r\n");
	mg_printf(conn, "Content-Type: application/binary\r\n");
    mg_printf(conn, "Cache: no-cache\r\n");
    mg_printf(conn, "Content-Length: %d\r\n",32000+32+1);        // Always set Content-Length 
    //std::string sHeader="Content-Disposition: attachment; filename=\""+sFileName+"\"\r\n";
    //mg_printf(conn, sHeader.c_str());
    mg_printf(conn, "\r\n"); 
	
    unsigned char iResolution = pxScreencastService->getSTResolution();
	mg_write(conn, &iResolution, 1);

	//copy palette
    pxScreencastService->getPalette(pxPalette);
	mg_write(conn, pxPalette, 16*2);

	//copy screen
    pxScreencastService->getScreen(pxScreen);
	mg_write(conn, pxScreen, 32000);
	return true;	
}

bool ScreencastController::getpaletteAction(mg_connection *conn, mg_request_info *req_info) 
{
    mg_printf(conn, "HTTP/1.1 200 OK\r\n");
	mg_printf(conn, "Content-Type: application/binary\r\n");
    mg_printf(conn, "Cache: no-cache\r\n");
    mg_printf(conn, "Content-Length: %d\r\n",16*2);        // Always set Content-Length 
    //std::string sHeader="Content-Disposition: attachment; filename=\""+sFileName+"\"\r\n";
    //mg_printf(conn, sHeader.c_str());
    mg_printf(conn, "\r\n"); 
	
	//copy palette
    pxScreencastService->getPalette(pxPalette);
    mg_write(conn, pxPalette, 16*2);

	return true;	
}
