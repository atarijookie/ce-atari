#ifndef _CDIRENTRY_H_
#define _CDIRENTRY_H_

#define _MAX_PATH   256
#define _MAX_FNAME  256

#include "../datatypes.h"

class CDirectory;                   // forward declaration

struct	LFN
{
	char			sName[8];
	char			sExt[3];
	unsigned char	attrib;
	unsigned char	pad[10];		// dummy pad (not used on FAT12)
	unsigned short	updateTime;
	unsigned short	updateDate;
	unsigned short	firstCluster;
	unsigned long	fileSize;
};

class CDirEntry
{
public:
	CDirEntry();
	~CDirEntry();

	void				Create(struct dirent *de, CDirectory *pSubDir, const char *pHostName);

	CDirectory		*	GetDirectory()		{ return m_pDirectory; }

	const char		*	GetHostName() const	{ return m_sHostName; }

	CDirEntry		*	GetNext()		{ return m_pNext; }
	void				SetNext(CDirEntry *pNext)		{ m_pNext = pNext; }

	void				LFN_Create(LFN *pLFN, int clusterStart);

    void                toUpper(char *str, int maxLen);

public:
	char				m_sHostName[_MAX_PATH];

	char*				m_pFileData;

	CDirectory		*	m_pDirectory;
	CDirEntry		*	m_pNext;
    
    char    sName[16];
    char    sExt[16];

    WORD    atariTime;
    WORD    atariDate;
    DWORD   fileSize;

    bool    isDir;
};

#endif
