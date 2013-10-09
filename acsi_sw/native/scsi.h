#ifndef SCSI_H
#define SCSI_H

#include "acsidatatrans.h"

#include "datatypes.h"

class Scsi
{
public:
    Scsi(void);

    void setAcsiDataTrans(AcsiDataTrans *dt);
    void processCommand(BYTE *command);

private:
    AcsiDataTrans *dataTrans;

    BYTE    shitHasHappened;

    struct {
        BYTE 	ACSI_ID;			// ID on the ACSI bus - from 0 to 7
        BYTE 	Type;				// DEVICETYPE_...
        bool	IsInit;				// is initialized and working? TRUE / FALSE
        bool	MediaChanged;		// when media is changed

        DWORD	BCapacity;			// device capacity in bytes
        DWORD	SCapacity;			// device capacity in sectors

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
