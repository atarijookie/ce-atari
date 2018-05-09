#include "webserver/webserver.h"

#include <string>
#include <sstream>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <linux/input.h>
#include <unistd.h>

#include "lib/cjson-code-58/cJSON.h"
#include "debug.h"
#include "global.h"
#include "downloadresource.h"

DownloadResource::DownloadResource()
{
}

DownloadResource::~DownloadResource()
{
}

bool DownloadResource::dispatch(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo)
{
	const char *path = sResourceInfo.c_str();

    Debug::out(LOG_DEBUG, "DownloadResource::dispatch -- %s", path);

    if( strstr(req_info->request_method,"GET")==0 && strstr(req_info->request_method,"POST")==0 && strstr(req_info->request_method,"PUT")==0 ){
        mg_printf(conn, "HTTP/1.1 405 Method Not Allowed %s\r\n",req_info->request_method);
        return true;
    }

	// for GET methods
    if( strstr(req_info->request_method,"GET")>0 ){
        // standard header on response
        mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n");
        mg_printf(conn, "Cache: no-cache\r\n");

        if(strcmp(path, "havefiles") == 0) {		// download/havefiles -- return list of files we already have
            std::ostringstream stringStream;
            stringStream << "{\"havefiles\": [\"A_008.zip\", \"A_013.zip\", \"A_020.zip\"]}";

            std::string sJson = stringStream.str();
            mg_printf(conn, "Content-Length: %lu\r\n\r\n",(unsigned long) sJson.length());	// Always set Content-Length
            mg_write(conn, sJson.c_str(), sJson.length());	// send content
            return true;
        }

        if(strcmp(path, "havelists") == 0) {		// download/havelists -- return list of lists of games we can search through
            std::ostringstream stringStream;
            stringStream << "{\"havelists\": [ {\"list_is_floppy_images\": 1, \"list_name_humanreadable\": \"games on exxos\", \"list_filename\": \"automation_exxos\"}, {\"list_is_floppy_images\": 1, \"list_name_humanreadable\": \"games on stonish\", \"list_filename\": \"stonish_games\"} ]}";

            std::string sJson = stringStream.str();
            mg_printf(conn, "Content-Length: %lu\r\n\r\n",(unsigned long) sJson.length());	// Always set Content-Length
            mg_write(conn, sJson.c_str(), sJson.length());	// send content
            return true;
        }

        if(strcmp(path, "imagelist") == 0) {		// download/imagelist -- return list images which we can search trhough
            std::ostringstream stringStream;
            stringStream << "{\"imagelist\": [{\"list_filename\":\"automation_exxos\", \"csv\": \"http://www.exxoshost.co.uk/atari/games/automation/files/A_000.zip,0x0000,Sinbad and the Throne of the Falcon,Myth\nhttp://www.exxoshost.co.uk/atari/games/automation/files/A_001.zip,0x0000,Crystal Castles,Star Wars: The Empire Strikes Back,Northstar w/ docs,Star Raiders,Robotron: 2084,Trail Blazer,Football Manager: World Cup Edition\nhttp://www.exxoshost.co.uk/atari/games/automation/files/A_002.zip,0x0000,Into the Eagle's Nest,Trantor: The Last Stormtrooper,Extensor,Knightmare,Plutos,Virus,Joe Blade,Trifide II\n\"},\
                                              {\"list_filename\":\"stonish_games\",    \"csv\": \"http://www.stonish.net/download/ACO_A.ZIP,0x0000,Vroom,Grav,FastCopy Pro 1.0c,FastCopy Pro 1.0c (doc)\nhttp://www.stonish.net/download/ACO_B.ZIP,0x0000,Flight Of The Intruder,Ripper 3.1,Musix Ripper 2.0,Low Res Grapic Ripper\nhttp://www.stonish.net/download/ACE5.ZIP,0x0000,Boston Bomb Club,Head Over Heels,The Blues Brothers,Neochrome Master 2.25\n\"} ]}";

            std::string sJson = stringStream.str();
            mg_printf(conn, "Content-Length: %lu\r\n\r\n",(unsigned long) sJson.length());	// Always set Content-Length
            mg_write(conn, sJson.c_str(), sJson.length());	// send content
            return true;
        }
	}

    return false;
}
