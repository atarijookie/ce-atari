// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/joystick.h>

#include <signal.h>
#include <pthread.h>

#include "utils.h"
#include "debug.h"

#include "global.h"
#include "periodicthread.h"
#include "floppy/imagesilo.h"
#include "ccorethread.h"
#include "native/scsi.h"
#include "native/scsi_defs.h"
#include "update.h"
#include "statusreport.h"
#include "json.h"

using json = nlohmann::json;

extern THwConfig        hwConfig;
extern TFlags           flags;
extern SharedObjects    shared;

void handleFloppyAction(std::string& action, json& data);
void handleGenericAction(std::string& action, json& data);
void handleIkbdAction(std::string& action, json& data);
void handleSceencastAction(std::string& action, json& data);
void closeFifo(bool keybNotMouse);

int createSocket(void)
{
	// create a UNIX DGRAM socket
	int sock = socket(AF_UNIX, SOCK_DGRAM, 0);

	if (sock < 0) {
	    Debug::out(LOG_ERROR, "cmdSockThreadCode: Failed to create command Socket!");
	    return -1;
	}

    std::string sockPath = Utils::dotEnvValue("CORE_SOCK_PATH");
    unlink(sockPath.c_str());       // delete sock file if exists

    struct sockaddr_un addr;
    strcpy(addr.sun_path, sockPath.c_str());
    addr.sun_family = AF_UNIX;

    int res = bind(sock, (struct sockaddr *) &addr, strlen(addr.sun_path) + sizeof (addr.sun_family));
    if (res < 0) {
	    Debug::out(LOG_ERROR, "cmdSockThreadCode: Failed to bind command socket to %s - errno: %d", sockPath.c_str(), errno);
	    return -1;
    }

    return sock;
}

void *cmdSockThreadCode(void *ptr)
{
    Debug::out(LOG_INFO, "Command Socket thread starting...");
    int sock = createSocket();

    if(sock < 0) {              // without socket this thread has no use
        return 0;
    }

    char bfr[1024];

    while(sigintReceived == 0) {
        struct timeval timeout;
        timeout.tv_sec = 1;                             // short timeout
        timeout.tv_usec = 0;

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        int res = select(sock + 1, &readfds, NULL, NULL, &timeout);     // wait for data or timeout here

        if(res < 0 || !FD_ISSET(sock, &readfds)) {          // if select() failed or cannot read from fd, skip rest
            continue;
        }

        if(bfr[0] != 0) {           // if buffer doesn't seem to be empty, clear it now
            memset(bfr, 0, sizeof(bfr));
        }

        ssize_t recvCnt = recv(sock, bfr, sizeof(bfr), 0);  // receive now

        if(recvCnt < 1) {                                   // nothing received?
            continue;
        }

        Debug::out(LOG_DEBUG, "cmdSockThreadCode: received: %s", bfr);

        json data;
        try {
            data = json::parse(bfr);   // try to parse the message
        }
        catch(...)                          // on any exception - log it, don't crash
        {
            std::exception_ptr p = std::current_exception();
            Debug::out(LOG_ERROR, "json::parse raised an exception: %s", (p ? p.__cxa_exception_type()->name() : "null"));
        }

        if(data.contains("module") && data.contains("action")) {    // mandatory fields found?
            std::string module = data["module"].get<std::string>();
            std::string action = data["action"].get<std::string>();

            Debug::out(LOG_DEBUG, "cmdSockThreadCode: module: %s, action: %s", module.c_str(), action.c_str());

            if(module == "floppy") {                // for floppy module?
                handleFloppyAction(action, data);
            } else if (module == "all") {           // generic / all modules?
                handleGenericAction(action, data);
            } else if(module == "ikbd") {           // ikbd module?
                handleIkbdAction(action, data);
            } else if(module == "screencast") {
                handleSceencastAction(action, data);
            } else {                                // for uknown module?
                Debug::out(LOG_WARNING, "cmdSockThreadCode: uknown module '%s', ignoring message!", module.c_str());
            }
        } else {        // some mandatory field is missing?
            Debug::out(LOG_WARNING, "cmdSockThreadCode: module or action is missing in the received data, ignoring message!");
        }
    }

    closeFifo(true);
    closeFifo(false);

    Debug::out(LOG_INFO, "Command Socket thread terminated.");
    return 0;
}

void handleFloppyAction(std::string& action, json& data)
{
    int slot = -1;
    pthread_mutex_lock(&shared.mtxImages);      // lock floppy images shared objects

    if(data.contains("slot")) {                 // if can get slot, get slot number
        slot = data["slot"].get<int>();
    }

    if(action == "insert") {                    // for 'insert' action
        if(data.contains("image")) {            // image is present in message
            std::string empty;
            std::string pathAndFile = data["image"].get<std::string>();     // get image full path

            std::string path, file;
            Utils::splitFilenameFromPath(pathAndFile, path, file);          // get just filename from full path

            shared.imageSilo->add(slot, file, pathAndFile, empty, true);    // insert into slot
        } else {
            Debug::out(LOG_WARNING, "handleFloppyAction: missing 'image' in message, ignoring message!");
        }
    } else if(action == "eject") {              // for 'eject' action
        shared.imageSilo->remove(slot);
    } else if(action == "activate") {           // for 'activate' action
        shared.imageSilo->setCurrentSlot(slot);
    } else {
        Debug::out(LOG_WARNING, "handleFloppyAction: unknown action '%s', ignoring message!", action.c_str());
    }

    pthread_mutex_unlock(&shared.mtxImages);    // unlock floppy images shared objects
}

void handleGenericAction(std::string& action, json& data)
{
    if(action == "generate_status") {
        StatusReport *sr = new StatusReport();
        sr->createReportFileFromEnv();
        delete sr;
    } else if(action == "set_loglevel") {
        int loglevel = 1;

        if(data.contains("loglevel")) {            // loglevel is present in message
            loglevel = data["loglevel"].get<int>();
            Debug::setLogLevel(loglevel);
        } else {
            Debug::out(LOG_WARNING, "handleGenericAction: missing 'loglevel' in message, ignoring message!");
        }
    } else {
        Debug::out(LOG_WARNING, "handleGenericAction: unknown action '%s', ignoring message!", action.c_str());
    }
}

int fdVirtKeyboard;
int fdVirtMouse;

int openFifo(bool keybNotMouse)
{
    int& fd = keybNotMouse ? fdVirtKeyboard : fdVirtMouse;
    std::string devpath = keybNotMouse ? Utils::dotEnvValue("IKBD_VIRTUAL_KEYBOARD_FILE") : Utils::dotEnvValue("IKBD_VIRTUAL_MOUSE_FILE");

    if(fd > 0) {        // already got fd? just return it
        return fd;
    }

    Debug::out(LOG_DEBUG, "Creating %s", devpath.c_str());

    std::string vdevFolder = Utils::dotEnvValue("IKBD_VIRTUAL_DEVICES_PATH");
    Utils::mkpath(vdevFolder.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);   // make dir where the virtual devs will be

    // try to remove if it's a file
    unlink(devpath.c_str());

    int err = mkfifo(devpath.c_str(), 0666);

    if(err < 0) {
        Debug::out(LOG_ERROR, "Could not create %s - errno: %d", devpath.c_str(), errno);

		if(errno == EEXIST) {
            Debug::out(LOG_ERROR, "    %s already exists.", devpath.c_str());
		}
	}

    fd = open(devpath.c_str(), O_RDWR | O_NONBLOCK);    // we open Read/Write in order to initialize the fifo

    if(fd < 0) {
        Debug::out(LOG_ERROR, "openFifo(): open(%s) : %s", devpath.c_str(), strerror(errno));
    }

    return fd;
}

void closeFifo(bool keybNotMouse)
{
    int& fd = keybNotMouse ? fdVirtKeyboard : fdVirtMouse;

    if(fd <= 0) {       // nothing to close, quit
        return;
    }

    close(fd);
    fd = -1;
}

void sendKeyboardPacket(int iKeyCode, int iState)
{
    int fd = openFifo(true);

    input_event xEvent;
    gettimeofday(&xEvent.time, NULL);
    xEvent.type = EV_KEY;
    xEvent.code = iKeyCode;
    xEvent.value = iState;        // ev->value -- 1: down, 2: auto repeat, 0: up
    write(fd, &xEvent, sizeof(xEvent));
}

void sendMouseButton(int iButton,int iState)
{
    int fd = openFifo(false);

	ssize_t res;
    input_event xEvent;
    gettimeofday(&xEvent.time, NULL);
    xEvent.type = EV_KEY;

    if(iButton == 0) {
        xEvent.code = BTN_LEFT;
    } else if(iButton==1) {
        xEvent.code = BTN_RIGHT;
    } // TODO : other buttons ?

    xEvent.value = iState;

    res = write(fd, &xEvent, sizeof(xEvent));
	if(res < 0 ) {
		Debug::out(LOG_ERROR, "sendMouseButton() write: %s", strerror(errno));
	}
}

void sendMousePacket(int iX, int iY)
{
    int fd = openFifo(false);

	ssize_t res;

    input_event ev[2];
    gettimeofday(&ev[0].time, NULL);
	memcpy(&ev[1].time, &ev[0].time, sizeof(ev[0].time));
    ev[0].type = EV_REL;
    ev[0].code = REL_X;
    ev[0].value = iX;
    ev[1].type = EV_REL;
    ev[1].code = REL_Y;
    ev[1].value = iY;
    res = write(fd, ev, sizeof(ev));

	if(res < 0) {
		Debug::out(LOG_ERROR, "sendMousePacket() write: %s", strerror(errno));
	}
}

void handleIkbdAction(std::string& action, json& data)
{
    if(action == "mouse") {
        /*
        example data:
            {'module': 'ikbd', 'action': 'mouse', 'type': 'relative', 'x': -1, 'y': 11}
            {'module': 'ikbd', 'action': 'mouse', 'type': 'relative', 'x': 0, 'y': 6}
            {'module': 'ikbd', 'action': 'mouse', 'type': 'buttonleft', 'state': 'down'}
            {'module': 'ikbd', 'action': 'mouse', 'type': 'buttonleft', 'state': 'up'}
            {'module': 'ikbd', 'action': 'mouse', 'type': 'buttonright', 'state': 'down'}
            {'module': 'ikbd', 'action': 'mouse', 'type': 'buttonright', 'state': 'up'}
        */

        if(data.contains("type")) {
            std::string type = data["type"].get<std::string>();

            if(type == "relative") {
                if(data.contains("x") && data.contains("y")) {
                    int x = data["x"].get<int>();
                    int y = data["y"].get<int>();

                    sendMousePacket(x, y);      // send mouse packet now
                } else {
                    Debug::out(LOG_WARNING, "handleIkbdAction: mouse - relative - missing 'x' or 'y' in message, ignoring message!");
                }
            } else if(type == "buttonleft" || type == "buttonright") {
                if(data.contains("state")) {
                    std::string stateStr = data["state"].get<std::string>();
                    int state = (stateStr == "down") ? 1 : ((stateStr == "up") ? 0 : -1);           // down -> 1, up -> 0, others: -1
                    int button = (type == "buttonright") ? 1 : ((type == "buttonleft") ? 0 : -1);   // right -> 1, left -> 0, others: -1

                    if(button != -1) {      // got valid mouse button?
                        sendMouseButton(button, state);
                    } else {                // invalid mouse button
                        Debug::out(LOG_WARNING, "handleIkbdAction: mouse - button - invalid type in message, ignoring message!");
                    }
                } else {
                    Debug::out(LOG_WARNING, "handleIkbdAction: mouse - button - missing 'state' in message, ignoring message!");
                }
            }
        } else {
            Debug::out(LOG_WARNING, "handleIkbdAction: mouse - missing 'type' in message, ignoring message!");
        }
    } else if(action == "keyboard") {
        /*
        example data:
            {'module': 'ikbd', 'action': 'keyboard', 'type': 'pc', 'code': 35, 'state': 'down'}
            {'module': 'ikbd', 'action': 'keyboard', 'type': 'pc', 'code': 35, 'state': 'up'}
        */

        if(data.contains("code") && data.contains("state")) {
            int code = data["code"].get<int>();
            std::string stateStr = data["state"].get<std::string>();
            int state = (stateStr == "down") ? 1 : ((stateStr == "up") ? 0 : -1);   // down -> 1, up -> 0, others: -1

            if(state == -1) {       // invalid state?
                Debug::out(LOG_WARNING, "handleIkbdAction: keyboard - invalid state '%s', ignoring message!", stateStr.c_str());
            } else {                // goot state? ship it
                sendKeyboardPacket(code, state);
            }
        } else {
            Debug::out(LOG_WARNING, "handleIkbdAction: keyboard - missing 'code' or 'state' in message, ignoring message!");
        }

    } else {
        Debug::out(LOG_WARNING, "handleIkbdAction: unknown action '%s', ignoring message!", action.c_str());
    }
}

void handleSceencastAction(std::string& action, json& data)
{
    if(action == "do_screenshot") {                     // take a screenshot?
        events.doScreenShot = true;
    } else if(action == "screenshot_vbl_enable") {      // enable screenshot VBLs
        Utils::screenShotVblEnabled(true);
    } else if(action == "screenshot_vbl_disable") {     // disable screenshot VBLs
        Utils::screenShotVblEnabled(false);
    } else {
        Debug::out(LOG_WARNING, "handleSceencastAction: unknown action '%s', ignoring message!", action.c_str());
    }
}
