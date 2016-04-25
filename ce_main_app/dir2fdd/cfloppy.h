#ifndef _CFLOPPY_H_
#define _CFLOPPY_H_

#include "cdirentry.h"

//--------------- Disk geometry ----------------------------------------
static	const	int		NB_SECTOR_PER_TRACK	=	9;
static	const	int		NB_CYLINDER			=	80;
static	const	int		NB_HEAD				=	2;

//--------------- Filesystem geometry ----------------------------------
static	const	int		MAX_ROOT_ENTRY		=	112;
static	const	int		SECTOR_PER_FAT		=	5;
static	const	int		ROOTDIR_NBSECTOR	=	(MAX_ROOT_ENTRY*32)/512;

struct MSAHEADER
{
	unsigned short  ID;
    unsigned short  Sectors;
    unsigned short  Sides;
    unsigned short  StartTrack;
    unsigned short  EndTrack;
};

class CFloppy
{
public:
	CFloppy();
	~CFloppy();

	bool			Create(int nbSide,int nbSectorPerTrack,int nbCylinder);
	void			Destroy();

	bool			Fill(CDirectory *pRoot, char *floppyName);
	bool			WriteImage(const char *pName);

private:

	void			FAT_Flush();
	bool			BuildDirectory(LFN *pLFN,CDirectory *pDir,int cluster,int parentCluster,int size,int level);
	unsigned char *	GetRawAd(int cluster);
	void			w8(int offset,unsigned char d)		{ m_pRawImage[offset] = d; }
	void			w16(int offset,unsigned short d)	{ m_pRawImage[offset] = d&0xff; m_pRawImage[offset+1] = (d>>8); }

    int             ComputeRLE(unsigned char *p,unsigned char data,int todo);
    unsigned short  SWAP16(unsigned short d);
    void            LFNStrCpy(char *pDst,const char *pSrc,int len);
    
	int					m_nbSide;
	int					m_nbCylinder;
	int					m_nbSectorPerTrack;
	int					m_rawSize;

	CDirectory		*	m_pRoot;
	unsigned char	*	m_pRawImage;

	int					m_nbFreeCluster;
	int					m_nextCluster;
	int					m_maxFatEntry;
	int					m_nbFatEntry;
	int				*	m_pFat;

    char            floppyName[16];

};

#endif
