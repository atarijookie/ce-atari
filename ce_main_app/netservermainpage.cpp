// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "utils.h"
#include "global.h"
#include "debug.h"
#include "update.h"

#include "statusreport.h"
#include "netservermainpage.h"
#include "main_netserver.h"

extern TCEServerStatus serverStatus[MAX_SERVER_COUNT];

void NetServerMainPage::create(std::string &page, uint8_t *serverIp)
{
    page  = "<html><head><title>CosmosEx network server</title></head><body>\n";
    page += "<center><h1>CosmosEx network server</h1><br>\n";
    page += "Click on a link to open web page of individual device.<br><br>\n";
    page += "<center> <table border=1 cellspacing=0 cellpadding=5>\n";
    page += "<tr><td><center>#</center></td> <td>Device</td> <td>Status</td></tr>\n";

    uint8_t *ip = serverIp;                 // init to server IP
    uint8_t localhost[4] = {127, 0, 0, 1};  // localhost ip

    // if server ip is zero, use localhost
    if(serverIp[0] == 0 && serverIp[1] == 0 && serverIp[2] == 0 && serverIp[3] == 0) {
        ip = localhost;
    }

    char tmp[64];
    char url[100];

    for(int i=0; i<MAX_SERVER_COUNT; i++) {
        if(serverStatus[i].status == SERVER_STATUS_NOT_RUNNING) {   // skip not running servers
            continue;
        }

        sprintf(url, "<a href='http://%d.%d.%d.%d:%d'>", ip[0], ip[1], ip[2], ip[3], 81 + i);
        std::string ahref = std::string(url);

        page += "<tr><td><center>";

        // server index
        sprintf(tmp, "%d", i+1);
        page += tmp;

        page += "</center></td>\n  <td>";
        page += ahref;

        // device name
        page += "Device Name";

        page += "</a> </td>\n  <td>";
        page += ahref;

        // device status
        page += (serverStatus[i].status == SERVER_STATUS_OCCUPIED) ? "connected" : "waiting for device";

        page += "</a> </td>  </tr>\n";
    }

    page += "</table> </center>\n";
    page += "</body></html>\n";
}
