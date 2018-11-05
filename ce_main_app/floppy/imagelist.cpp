#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>

#include <unistd.h>

#include "imagelist.h"
#include "imagesilo.h"
#include "imagestorage.h"
#include "../utils.h"
#include "../periodicthread.h"
#include "../downloader.h"
#include "../debug.h"

extern SharedObjects shared;

ImageList::ImageList(void)
{
    isLoaded = false;
}

bool ImageList::exists(void)
{
    int res = access(IMAGELIST_LOCAL, F_OK);            // check if file exists

    if(res == 0) {
        return true;
    }

    // ok, so the file does not exist
    int cnt = Downloader::count(DWNTYPE_FLOPPYIMG_LIST); // check if it's downloaded at this moment

    if(cnt > 0) {                                       // the file is being downloaded, but we don't have it yet
        return false;
    }

    downloadFromWeb();      // don't have it, not downloading? download!
    return false;
}

void ImageList::downloadFromWeb(void)
{
    // start the download
    TDownloadRequest tdr;
    tdr.srcUrl          = IMAGELIST_URL;
    tdr.checksum        = 0;                            // don't check checksum
    tdr.dstDir          = IMAGELIST_LOCAL_DIR;
    tdr.downloadType    = DWNTYPE_FLOPPYIMG_LIST;
    tdr.pStatusByte     = NULL;                     // don't update this status byte
    Downloader::add(tdr);
}

bool ImageList::getIsLoaded(void)
{
    return isLoaded;
}

bool ImageList::loadList(void)
{
    vectorOfImages.clear();
    isLoaded = false;

    FILE *f = fopen(IMAGELIST_LOCAL, "rt");             // open file

    if(!f) {
        return false;
    }

    char tmp[1024];
    fgets(tmp, 1023, f);                                // skip version line

    ImageListItem li;                                   // store url and checksum in structure
    int n=0;

    while(!feof(f)) {
        n++;
        memset(tmp, 0, 1024);

        char *c = fgets(tmp, 1023, f);                  // read one line

        if(c == NULL) {                                 // didn't read anything?
            continue;
        }

        int len = strlen(tmp);      // get length of string
        int i;
        for(i=0; i<len; i++) {      // replace all line terminators with zero
            if(tmp[i] == '\n' || tmp[i] == '\r') {
                tmp[i] = 0;
            }

            if(tmp[i] == '"') {    // if it's double quote, replace with single quote (double quote is used in json)
                tmp[i] = '\'';
            }
        }

        char *tok;
        tok = strtok(tmp, ",");                         // init strtok by passing pointer to string

        if(tok == NULL) {
            continue;
        }

        li.url = tok;                                   // store URL

        std::string path, file;
        Utils::splitFilenameFromPath(li.url, path, file);
        li.imageName = file;                            // also store only image name (without URL)

        tok = strtok(NULL, ",");                        // get next token - checksum

        if(tok == NULL) {
            continue;
        }

        int checksum, res;
        res = sscanf(tok, "0x%x", &checksum);           // get the checksum

        if(res != 1) {
            continue;
        }

        li.checksum = checksum;                         // store checksum

        while(1) {                                      // now move beyond checksum by looking for 0 as string terminator
            if(*tok == 0) {
                break;
            }
            tok++;
        }

        tok++;                                          // tok now points to the start of games string

        if(*tok == 0) {                                 // nothing in this image? skip it
            continue;
        }

        li.games = tok;                                                                     // store games
        std::transform(li.games.begin(), li.games.end(), li.games.begin(), ::tolower);      // convert them to lowercase
        std::replace(li.games.begin(), li.games.end(), '\t', ' ');
        std::replace(li.games.begin(), li.games.end(), '\\', ' ');
        std::replace(li.games.begin(), li.games.end(), '/', ' ');
        li.marked = false;

//      Debug::out(LOG_DEBUG, "[%d] - %s - %s - %s", n, li.url.c_str(), li.imageName.c_str(), li.games.c_str());

        vectorOfImages.push_back(li);                   // store it in vector
    }

    fclose(f);

    isLoaded = true;
    return true;
}

void ImageList::search(const char *part)
{
    std::string sPart = part;
    std::transform(sPart.begin(), sPart.end(), sPart.begin(), ::tolower);                   // convert to lowercase

    vectorOfResults.clear();                                        // clear results

    int cnt = vectorOfImages.size();

    // if not loaded, try to load
    if(!isLoaded) {
        if(!loadList()) {   // try to load list, but if failed, quit
            return;
        }
    }

    if(cnt < 1) {           // list empty? quit
        return;
    }

    // if search string is too short
    bool searchTooShort = false;
    if(strlen(part) < 2) {
        searchTooShort = true;
    }

    // go through the list of images, copy the right ones
    for(int i=0; i<cnt; i++) {
        std::string &games = vectorOfImages[i].games;
        std::string game;
        SearchResult sr;

        if(searchTooShort) {
            sr.game         = games;                                // store stuff in search result structure
            sr.imageIndex   = i;

            vectorOfResults.push_back(sr);                          // store search result in vector
            continue;                                               // skip searching of the games string
        }

        size_t pos = 0;                                             // start searching from start
        while(1) {
            pos = games.find(sPart, pos);                           // search for part in games

            if(pos == std::string::npos) {                          // not found? quit this
                break;
            }

            getSingleGame(games, game, pos);                        // extract single game from list of games

            sr.game         = game;                                 // store stuff in search result structure
            sr.imageIndex   = i;

            vectorOfResults.push_back(sr);                          // store search result in vector

            pos++;                                                  // move to next char
        }
    }
}

bool ImageList::getImageUrl(const char *imageFileName, std::string &url)
{
    std::string filename = imageFileName;
    url.clear();

    int cnt = vectorOfImages.size();

    // if not loaded, or the list is empty
    if(!isLoaded || cnt < 1) {
        return false;
    }

    // go through the list of images, look for filename
    for(int i=0; i<cnt; i++) {
        if(filename.compare(vectorOfImages[i].imageName) == 0) {    // if input filename and image name in list match, success
            url = vectorOfImages[i].url;                            // copy url and quit
            return true;
        }
    }

    // if came here, image not found
    return false;
}

void ImageList::getSingleGame(std::string &games, std::string &game, size_t pos)
{
    int len = games.length();

    if(len <= 0) {
        game = "";
        return;
    }

    int i;
    size_t start    = 0;
    size_t end      = (len - 1);

    for(i=pos; i>0; i--) {                                                      // find starting coma
        if(games[i] == ',') {
            start = i + 1;                                                      // store position without that coma
            break;
        }
    }

    for(i=pos; i<len; i++) {                                                    // find ending coma
        if(games[i] == ',') {
            end = i - 1;                                                        // store position without that coma
            break;
        }
    }

    if(start >= (size_t) len) {                                                 // if start would be out of range, fix it
        start = len - 1;
    }

    if(end >= (size_t) len) {                                                   // if end would be out of range, fix it
        end = len - 1;
    }

    game = games.substr(start, (end - start + 1));                              // get only the single game
}

void ImageList::getResultByIndex(int index, char *bfr, int screenResolution)
{
    // if not loaded, try to load
    if(!isLoaded) {
        if(!loadList()) {    // try to load list, but if failed, quit
            return;
        }
    }

    #define SIZE_OF_RESULT      68
    memset(bfr, 0, SIZE_OF_RESULT);                                             // clear the memory
    std::string out;

    if(index < 0 || index >= (int) vectorOfResults.size()) {                    // if out of range
        return;
    }

    int imageIndex = vectorOfResults[index].imageIndex;
    std::string imageName = vectorOfImages[imageIndex].imageName;

    bool weHaveThisImage = shared.imageStorage->weHaveThisImage(imageName.c_str());

    int len = imageName.length();               // get the lenght of this filename

    if(len <= 12) {                             // name shorter than 8+3? pad to length
        out += imageName;                       // add whole filename
        out.append(12 - len, ' ');              // pad with spaces
    } else {                                    // name too long? insert only first part of name
        out += imageName.substr(0, 12);
    }

    if(weHaveThisImage) {                       // if we have this image, show tick
        out += " \010 ";
    } else {                                    // don't have it, don't show tick
        out += "   ";
    }

    shared.imageSilo->containsImageInSlots(imageName, out);     // slots info

    out += " ";                                                 // space slots and list of games

    size_t colCount = (screenResolution == 0) ? 40 : SIZE_OF_RESULT;    // low res has 40 cols, mid and high res has 80 cols

    size_t lenOfRest = colCount - 1 - out.length();             // how much can we fit in ;
    if(vectorOfResults[index].game.length() <= lenOfRest) {     // games will fit in that buffer?
        out += vectorOfResults[index].game;                     // add all content there
    } else {                                                    // content won't fit? copy just part
        out += vectorOfResults[index].game.substr(0, lenOfRest);
    }

    strncpy(bfr, out.c_str(), SIZE_OF_RESULT - 1);              // the output in the buffer
}

void ImageList::getResultByIndex(int index, std::ostringstream &stream)
{
    if(index < 0 || index >= (int) vectorOfResults.size()) {	// if out of range
        return;
    }

    int imageIndex = vectorOfResults[index].imageIndex;

	stream << "{ \"imageName\": \"";
    stream << vectorOfImages[imageIndex].imageName.c_str();	// copy in the name of image
	stream << "\",";

	// check if we got this image downloaded, set true/false according to that bellow
	bool weHaveThisImage = shared.imageStorage->weHaveThisImage(vectorOfImages[imageIndex].imageName.c_str());

	stream << "\"haveIt\": ";
    if(weHaveThisImage) {									// if image is downloaded and is ready for insertion
        stream << "true";
    } else {
        stream << "false";
    }

	stream << ", \"content\": \"";

    stream <<vectorOfResults[index].game.c_str();	// copy in the name of game

	stream << "\"}";
}

void ImageList::markImage(int index)
{
    if(index < 0 || index >= (int) vectorOfResults.size()) {                    // if out of range
        return;
    }

    int imageIndex = vectorOfResults[index].imageIndex;                         // get image index for selected result

    if(imageIndex < 0 || imageIndex >= (int) vectorOfImages.size()) {           // if out of range
        return;
    }

    vectorOfImages[imageIndex].marked = !vectorOfImages[imageIndex].marked;     // toggle the marked flag
}

bool ImageList::getImageNameFromResultsByIndex(int index, std::string &imageName)
{
    imageName.clear();

    if(index < 0 || index >= (int) vectorOfResults.size()) {                    // if out of range
        return false;
    }

    int imageIndex = vectorOfResults[index].imageIndex;                         // get image index for selected result

    if(imageIndex < 0 || imageIndex >= (int) vectorOfImages.size()) {           // if out of range
        return false;
    }

    imageName = vectorOfImages[imageIndex].imageName;	// get image name into string
    return true;
}

int ImageList::getSearchResultsCount(void)
{
    return vectorOfResults.size();
}

bool ImageList::getFirstMarkedImage(std::string &url, int &checksum, std::string &filename)
{
    int cnt = (int) vectorOfImages.size();

    for(int i=0; i<cnt; i++) {                                                  // go through images and see which one is checked
        if(vectorOfImages[i].marked) {
            url         = vectorOfImages[i].url;
            checksum    = vectorOfImages[i].checksum;
            filename    = vectorOfImages[i].imageName;

            vectorOfImages[i].marked = false;                                   // unmark it, return success
            return true;
        }
    }

    url         = "";
    checksum    = 0;
    filename    = "";

    return false;                                                               // not found, fail
}

void ImageList::refreshList(void)
{
    unlink(IMAGELIST_LOCAL);                // delete current image list

    vectorOfImages.clear();                 // remove the loaded list from memory
    isLoaded = false;

    exists();                               // this should now start the new image list download
}
