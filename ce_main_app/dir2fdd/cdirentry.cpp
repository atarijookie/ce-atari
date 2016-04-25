#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include "cfloppy.h"
#include "cdirentry.h"
#include "cdirectory.h"

#include "../translated/filenameshortener.h"
#include "../utils.h"
#include "../debug.h"

CDirEntry::CDirEntry()
{
	m_pNext         = NULL;
	m_pDirectory    = NULL;
	m_pFileData     = NULL;
}

CDirEntry::~CDirEntry()
{
	if(m_pDirectory) {
		delete m_pDirectory;
    }
    
    if(m_pFileData) { 
        delete []m_pFileData;
    }
}

void CDirEntry::Create(struct dirent *de, CDirectory *pDir, const char *pHostName)
{
	m_pDirectory = pDir;

	if (!pHostName) {
        return;
    }

	int res;
	struct stat attr;
	tm *timestr;
    std::string shortFname;
	
	res = stat(pHostName, &attr);					            // get the file status
	
	if(res != 0) {
		Debug::out(LOG_ERROR, "CDirEntry::Create() -- stat() failed for %s, errno %d", pHostName, errno);
		return;		
	}

	timestr = localtime(&attr.st_mtime);			    	    // convert time_t to tm structure
    atariTime = Utils::fileTimeToAtariTime(timestr);
    atariDate = Utils::fileTimeToAtariDate(timestr);
    
    fileSize = attr.st_size;
    fileSize = (fileSize < (720 * 1024)) ? fileSize : (720 * 1024);

    strcpy(m_sHostName, pHostName);
    FILE* h = fopen( m_sHostName, "rb" );
    
    if(h) {
        m_pFileData = new char[fileSize + 1];	                // +1 to avoid problem with 0 bytes file
        fread( m_pFileData, 1, fileSize, h );
        fclose( h );
    } else {
        Debug::out(LOG_ERROR, "CDirEntry::Create() -- could not load %s", pHostName );
    }
    
	FilenameShortener::splitFilenameFromExtension(de->d_name, sName, sExt);
    toUpper(sName,  8);
    toUpper(sExt,   3);
    
    isDir = (de->d_type == DT_DIR);
}

void CDirEntry::toUpper(char *str, int maxLen) 
{
    int i;
    
    for(i=0; i<maxLen; i++) {
        if(str[i] < 32) {                       // replace non-chars with spaces
            str[i] = 32;
        }
        
        if(str[i] >= 'a' && str[i] <= 'z') {    // convert lower to upper caae
            str[i] -= 32;
        }
    }
}

void CDirEntry::LFN_Create(LFN *pLFN, int clusterStart)
{
	memset(pLFN,0,sizeof(LFN));

	memcpy(pLFN->sName, sName, 8);
	memcpy(pLFN->sExt,  sExt,  3);

	if (isDir) {
        pLFN->attrib = (1<<4);		// directory
    }

	pLFN->firstCluster = clusterStart;

	if (!isDir) {
		pLFN->fileSize = fileSize;
    }
    
    pLFN->updateDate = atariDate;
    pLFN->updateTime = atariTime;
}
