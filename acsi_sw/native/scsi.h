#ifndef SCSI_H
#define SCSI_H

#include "acsidatatrans.h"
#include "datamedia.h"
#include "nomedia.h"
#include "imedia.h"

#include "datatypes.h"

#define SCSI_TYPE_FULL          0
#define SCSI_TYPE_READ_ONLY     1
#define SCSI_TYPE_NO_DATA       2

class Scsi
{
public:
    Scsi(void);
    ~Scsi();

    void setAcsiDataTrans(AcsiDataTrans *dt);
    void setDataMedia(IMedia *dm);
    void setAcsiID(int newId);
    void setDeviceType(int newType);

    void processCommand(BYTE *command);

private:
    AcsiDataTrans   *dataTrans;
    IMedia          *dataMedia;

    NoMedia         noMedia;

    BYTE            *dataBuffer;
    BYTE            *dataBuffer2;

    BYTE    shitHasHappened;

    struct {
        BYTE 	ACSI_ID;			// ID on the ACSI bus - from 0 to 7
        BYTE 	type;				// SCSI_TYPE_FULL || SCSI_TYPE_READ_ONLY || SCSI_TYPE_NO_DATA

        BYTE	LastStatus;			// last returned SCSI status
        BYTE	SCSI_ASC;			// additional sense code
        BYTE	SCSI_ASCQ;			// additional sense code qualifier
        BYTE	SCSI_SK;			// sense key
    } devInfo;

    BYTE *cmd;
    BYTE inquiryName[10];

    bool isICDcommand(void);

	// for 6-byte long commands - from scsi6
    void ProcScsi6(void);

	void SCSI_RequestSense(void);
	void SCSI_FormatUnit(void);

    void SCSI_ReadWrite6(bool read);

	void SCSI_Inquiry(void);
	void SCSI_ModeSense6(void);

	void SendOKstatus(void);
	void ReturnStatusAccordingToIsInit(void);
	void ReturnUnitAttention(void);
	void ClearTheUnitAttention(void);
    void returnInvalidCommand(void);

	void SendEmptySecotrs(WORD sectors);

	// for commands longer than 6 bytes - from scsiICD
	void ProcICD(void); 

	void SCSI_ReadCapacity(void);
	void ICD7_to_SCSI6(void);
    void SCSI_ReadWrite10(bool read);
	void SCSI_Verify(void);
	
	void showCommand(WORD id, WORD length, WORD errCode);

    bool readSectors(DWORD sectorNo, DWORD count);
    bool writeSectors(DWORD sectorNo, DWORD count);
    bool compareSectors(DWORD sectorNo, DWORD count);
    bool eraseMedia(void);
};

#endif
