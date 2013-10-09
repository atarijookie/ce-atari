#ifndef SCSI_H
#define SCSI_H

#include "datatypes.h"

class CScsi
{
public:
	

private:
    BYTE    shitHasHappened;

    struct {
        BYTE 	ACSI_ID;			// ID on the ACSI bus - from 0 to 7
        BYTE 	Type;				// DEVICETYPE_...
        bool	IsInit;				// is initialized and working? TRUE / FALSE
        bool	MediaChanged;		// when media is changed

        BYTE	InitRetries;		// how many times try to init the device */

        DWORD	BCapacity;			// device capacity in bytes
        DWORD	SCapacity;			// device capacity in sectors

        BYTE	LastStatus;			// last returned SCSI status
        BYTE	SCSI_ASC;			// additional sense code
        BYTE	SCSI_ASCQ;			// additional sense code qualifier
        BYTE	SCSI_SK;			// sense key
    } devInfo;

    char *cmd;

	// for 6-byte long commands - from scsi6
	void ProcSCSI6(void); 

	void SCSI_RequestSense(void);
	void SCSI_FormatUnit(void);

	void SCSI_ReadWrite6(BYTE Read);

	BYTE SCSI_Read6_SDMMC(DWORD sector, WORD lenX);
	BYTE SCSI_Write6_SDMMC(DWORD sector, WORD lenX);

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
	void SCSI_ReadWrite10(char Read);
	void SCSI_Verify(void);
	
	void showCommand(WORD id, WORD length, WORD errCode);
};

#endif
