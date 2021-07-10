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

#include "../debug.h"
#include "../translated/filenameshortener.h"

CDirectory::CDirectory()
{
	m_nbEntry = 0;
	m_pEntryList = NULL;
}

CDirectory::~CDirectory()
{
	CDirEntry *pEntry = m_pEntryList;
	while (pEntry)
	{
		CDirEntry *pTmp = pEntry;
		pEntry = pEntry->GetNext();
		delete pTmp;
	}
}

void CDirectory::AddEntry(struct dirent *de, CDirectory *pSubDir, const char *pHostName)
{
	CDirEntry *pEntry = new CDirEntry;
    
	pEntry->Create(de, pSubDir, pHostName);
	pEntry->SetNext(m_pEntryList);
    
	m_pEntryList = pEntry;
	m_nbEntry++;
}

void CDirectory::DirectoryScan(const char *pDir)
{
	char tmpName[_MAX_PATH + 32];
	strcpy(tmpName,pDir);
	strcat(tmpName,"\\*.*");
	
	DIR *dir = opendir((char *) pDir);						        // try to open the dir
	
    if(dir == NULL) {                                 			    // not found?
        return;
    }
    
	while(1) {                                                  	// while there are more files, store them
		struct dirent *de = readdir(dir);							// read the next directory entry
	
		if(de == NULL) {											// no more entries?
			break;
		}
	
		if(de->d_type != DT_DIR && de->d_type != DT_REG) {			// not 	a file, not a directory?
			continue;
		}

		sprintf(tmpName,"%s/%s", pDir, de->d_name);

        if(de->d_type == DT_DIR) {                                  // for dir
            if(de->d_name[0] == '.') {                              // skip . and ..
                continue;
            }
            
            CDirectory *pNewDir = new CDirectory();
			this->AddEntry(de, pNewDir, tmpName);
            
			pNewDir->DirectoryScan(tmpName);
        } else {
            this->AddEntry(de, NULL, tmpName);
        }
    }

    closedir(dir);	
}

bool CDirectory::dir2fdd(char *sourceDirectory, char *fddImageName)
{
    Debug::out(LOG_INFO, "CDirectory::dir2fdd() -- will try to create %s from %s", fddImageName, sourceDirectory);
    
   	CDirectory *pDir = new CDirectory();
	pDir->DirectoryScan(sourceDirectory);

    if(!pDir) {
        Debug::out(LOG_ERROR, "CreateTreeFromDirectory failed...");
        return false;
    }
    
    CFloppy floppy;
    floppy.Create(NB_HEAD,NB_SECTOR_PER_TRACK,NB_CYLINDER);

    bool bOk = floppy.Fill( pDir, (char *) "CONF_FDD" );

    if (bOk) {
        Debug::out(LOG_INFO, "CDirectory::dir2fdd() -- Writing file %s", fddImageName);
        floppy.WriteImage(fddImageName);
    }

    delete pDir;
	return bOk;
}
