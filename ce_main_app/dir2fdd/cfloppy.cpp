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

CFloppy::CFloppy()
{
	m_pRawImage = NULL;
	m_pFat = NULL;
}

CFloppy::~CFloppy()
{
	Destroy();
}

void CFloppy::Destroy()
{
	if (m_pRawImage)
	{
		delete [] m_pRawImage;
		m_pRawImage = NULL;
	}

	if (m_pFat)
	{
		delete [] m_pFat;
		m_pFat = NULL;
	}
}

bool CFloppy::Create(int nbSide,int nbSectorPerTrack,int nbCylinder)
{
	Destroy();

	m_nbSide            = nbSide;
	m_nbCylinder        = nbCylinder;
	m_nbSectorPerTrack  = nbSectorPerTrack;
	m_rawSize           = nbSide * nbSectorPerTrack * nbCylinder * 512;

	m_pRawImage = new unsigned char[m_rawSize];

	if (m_pRawImage) {
		// Fill the raw image
//		memset(m_pRawImage, 0xe5, m_rawSize);
		memset(m_pRawImage, 0, m_rawSize);

		// Build the bootsector
		w16(0xb,512);				// byte per sector
		w8(0xd,2);					// sector per cluster
		w16(0xe,1);					// reserved sector (boot sector)
		w8(0x10,2);					// number of fat !! (2 fat per disk)
		w16(0x11,MAX_ROOT_ENTRY);	// Nb root entries
		w16(0x13,nbSide * nbSectorPerTrack * nbCylinder);	// total sectors
		w8(0x15,0xf7);				// media type
		w16(0x16,SECTOR_PER_FAT);	// sectors per fat
		w16(0x18,m_nbSectorPerTrack);
		w16(0x1a,m_nbSide);

		// atari specific
		w16(0x0,0xe9);
		w16(0x1c,0);
//		memset(m_pRawImage + 0x1e,0x4e,30);

		int nbFsSector      = 1 + SECTOR_PER_FAT * 2 + ROOTDIR_NBSECTOR;
		int nbDataSector    = nbSide * nbSectorPerTrack * nbCylinder - nbFsSector;
		m_maxFatEntry       = (nbDataSector / 2);
		m_nbFreeCluster     = m_maxFatEntry;
		m_nextCluster       = 2;
        
		m_pFat              = new int [m_maxFatEntry];
		memset(m_pFat, 0, m_maxFatEntry * sizeof(int));
	}

	return (NULL != m_pRawImage);
}

unsigned short CFloppy::SWAP16(unsigned short d)
{
	return ((d>>8) | (d<<8));
}

bool CFloppy::WriteImage(const char *pName)
{
	FILE *h = fopen(pName,"wb");
	
    if(!h) {
        return false;
    }
    
    FAT_Flush();

    MSAHEADER header;
    header.ID           = 0x0f0e;
    header.StartTrack   = 0;
    header.EndTrack     = SWAP16(m_nbCylinder-1);
    header.Sectors      = SWAP16(m_nbSectorPerTrack);
    header.Sides        = SWAP16(m_nbSide-1);
    fwrite(&header,1,sizeof(header),h);

    unsigned char *pTempBuffer = new unsigned char [32*1024];

    int rawSize = m_nbSectorPerTrack * 512;

    for (int t=0;t<m_nbCylinder * m_nbSide;t++)
    {
        unsigned char *pR = (unsigned char*)m_pRawImage + t * rawSize;
        unsigned char *pTemp = pTempBuffer;
        int todo = rawSize;
        
        while (todo > 0)
        {
            unsigned char data = *pR;
            
            int nRepeat = ComputeRLE(pR,data,todo);
            if ((nRepeat > 4) || (0xe5 == data))
            {	// RLE efficient (or 0xe5 special case)
                *pTemp++ = 0xe5;
                *pTemp++ = data;
                *pTemp++ = (nRepeat>>8)&255;
                *pTemp++ = (nRepeat&255);
                todo -= nRepeat;
                pR += nRepeat;
            }
            else
            {
                *pTemp++ = data;
                todo--;
                pR++;
            }
        }
        
        // check if packing track was efficient
        int packedSize = ((int)pTemp - (int)pTempBuffer);
        if (packedSize < rawSize)
        {
            fputc(packedSize>>8,h);
            fputc(packedSize&255,h);
            fwrite(pTempBuffer,1,packedSize,h);
        }
        else
        {
            fputc(rawSize>>8,h);
            fputc(rawSize&255,h);
            fwrite((unsigned char*)m_pRawImage + t * rawSize,1,rawSize,h);
        }
    }

    delete [] pTempBuffer;

    return true;
}

unsigned char * CFloppy::GetRawAd(int cluster)
{
	return m_pRawImage + 512 * (1+SECTOR_PER_FAT*2+ROOTDIR_NBSECTOR) + 1024*(cluster-2);		// always two reserved clusters
}

void CFloppy::LFNStrCpy(char *pDst,const char *pSrc,int len)
{
	for (int i=0; i<len; i++)
	{
		if ((*pSrc) && (*pSrc != '.'))
		{
			*pDst++ = *pSrc++;
		} else {
			*pDst++ = 0x20;
        }
	}
}

bool CFloppy::BuildDirectory(LFN *pLFN,CDirectory *pDir,int cluster,int parentCluster,int size,int level)
{
	// clear the directory file
	memset(pLFN,0,size);

	if (cluster > 0)
	{
		// Create "." directory
		memset(pLFN,0,sizeof(LFN));
		pLFN->attrib = 0x10;			// directory
		memset(pLFN->sName,0x20,8+3);
		strcpy(pLFN->sName,".");
		pLFN->firstCluster = cluster;
		pLFN++;

		// Create ".." directory
		memset(pLFN,0,sizeof(LFN));
		pLFN->attrib = 0x10;			// directory
		memset(pLFN->sName,0x20,8+3);
		strcpy(pLFN->sName,"..");
		pLFN->firstCluster = parentCluster;
		pLFN++;

	}

	CDirEntry *pEntry = pDir->GetFirstEntry();
	while (pEntry)
	{
		CDirectory *pSubDir = pEntry->GetDirectory();			
		if (pSubDir)
		{
			// reserve space for directory
			int nbCluster = (((pSubDir->GetNbEntry()+2)*32)+1023)/1024;		// nbentry+2 because of "." and ".." directory

			if (nbCluster > m_nbFreeCluster)
			{
				Debug::out(LOG_ERROR, "CFloppy::BuildDirectory: ERROR - No more space on the disk.");
				return false;
			}

			int SubDirCluster = m_nextCluster;

			pEntry->LFN_Create(pLFN,SubDirCluster);

			for (int i=0;i<nbCluster-1;i++) {
				m_pFat[SubDirCluster+i] = SubDirCluster+i+1;
            }

			m_pFat[SubDirCluster+nbCluster-1] = -1;		// end chain marker

			m_nextCluster += nbCluster;
			m_nbFreeCluster -= nbCluster;

			if (!BuildDirectory((LFN*)GetRawAd(SubDirCluster),pSubDir,SubDirCluster,cluster,nbCluster*1024,level+1))
				return false;

		}
		else
		{
			// create file entry
			int nbCluster = (pEntry->fileSize + 1023) / 1024;

			if (nbCluster > 0)
			{
				if (nbCluster > m_nbFreeCluster)
				{
					Debug::out(LOG_ERROR, "CFloppy::BuildDirectory - ERROR: No more space on the disk.");
					return false;
				}

				int fileCluster = m_nextCluster;
				pEntry->LFN_Create(pLFN,fileCluster);

				if ( pEntry->fileSize >= 0 ) {
					memcpy( GetRawAd(fileCluster), pEntry->m_pFileData, pEntry->fileSize );
				} else {
					Debug::out(LOG_ERROR, "CFloppy::BuildDirectory: - ERROR: Could not load host file %s",pEntry->GetHostName());
					return false;
				}

				for (int i=0;i<nbCluster-1;i++) {
					m_pFat[fileCluster+i] = fileCluster+i+1;
                }

				m_pFat[fileCluster+nbCluster-1] = -1;		// end chain marker
			} else {                                        // special case for 0 bytes files !!
				pEntry->LFN_Create(pLFN,0);				    // 0 byte file use "0" as first cluster
			}
            
			m_nextCluster += nbCluster;
			m_nbFreeCluster -= nbCluster;
		}

		pLFN++;
		pEntry = pEntry->GetNext();
	}
	return true;
}

void CFloppy::FAT_Flush()
{
	unsigned char *pFat = m_pRawImage + 512;
	memset(pFat,0,SECTOR_PER_FAT*512);

/*    
	pFat[0] = 0xf7;
	pFat[1] = 0xff;
	pFat[2] = 0xff;
*/

	unsigned char *p = pFat + 3;

	for (int i=2;i<m_maxFatEntry;i+=2)
	{

		unsigned int a = m_pFat[i]&0xfff;
		unsigned int b = 0;
		
		if ((i+1)<m_maxFatEntry)
			b = m_pFat[i+1]&0xfff;

		p[0] = a&0xff;
		p[1] = (a>>8) | ((b&0xf)<<4);
		p[2] = (b>>4);
		p += 3;
	}

	// duplicate second fat
	memcpy(pFat + SECTOR_PER_FAT*512,pFat,SECTOR_PER_FAT*512);
}

bool CFloppy::Fill(CDirectory *pRoot, char *floppyName)
{
    strcpy(this->floppyName, floppyName);

	if ((pRoot->GetNbEntry()+1) > MAX_ROOT_ENTRY)
	{
		Debug::out(LOG_ERROR, "CFloppy::Fill - ERROR: Too much files in root directory (%d > %d)\n", pRoot->GetNbEntry(), MAX_ROOT_ENTRY);
		return false;
	}

	LFN *pLFN = (LFN*)(m_pRawImage + 512 * (1+2*SECTOR_PER_FAT));

	// Root dir is special: there is a reserved space after boot and fats
	if (BuildDirectory(pLFN,pRoot,0,0,ROOTDIR_NBSECTOR * 512,0)) {
		return true;
	}

	return false;
}

int CFloppy::ComputeRLE(unsigned char *p,unsigned char data,int todo)
{
	int nb = 0;
	while ((todo > 0) && (*p == data))
	{
		nb++;
		todo--;
		p++;
	}
	return nb;
}
