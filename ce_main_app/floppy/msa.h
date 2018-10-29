/*
  Hatari - msa.h

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.
*/

#include "../datatypes.h"

#define Uint8   BYTE
#define Uint16  WORD

extern bool MSA_FileNameIsMSA(const char *pszFileName, bool bAllowGZ);
extern Uint8 *MSA_UnCompress(Uint8 *pMSAFile, long *pImageSize);
extern Uint8 *MSA_ReadDisk(const char *pszFileName, long *pImageSize);
extern bool MSA_WriteDisk(const char *pszFileName, Uint8 *pBuffer, int ImageSize);
bool File_Save(const char *pszFileName, const Uint8 *pAddress, int Size);
void Floppy_FindDiskDetails(const Uint8 *pBuffer, int nImageBytes, Uint16 *pnSectorsPerTrack, Uint16 *pnSides);
void Floppy_DoubleCheckFormat(long nDiskSize, long nSectorsPerDisk, Uint16 *pnSides, Uint16 *pnSectorsPerTrack);
