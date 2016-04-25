#ifndef _CDIRECTORY_H_
#define _CDIRECTORY_H_

#include "cdirentry.h"

class CDirectory
{
public:
	CDirectory();
	~CDirectory();

    static bool dir2fdd(char *sourceDirectory, char *fddImageName);
    CDirectory *CreateTreeFromDirectory(const char *pHostDirName);

    void DirectoryScan(const char *pDir);
	void AddEntry(struct dirent *de, CDirectory *pSubDir, const char *pHostName);

	int  GetNbEntry() const				{ return m_nbEntry; }
	CDirEntry	*	GetFirstEntry()	const	{ return m_pEntryList; }

	CDirectory*		DirExist()	const;

private:
	int				m_nbEntry;
	CDirEntry	*	m_pEntryList;
};

#endif
