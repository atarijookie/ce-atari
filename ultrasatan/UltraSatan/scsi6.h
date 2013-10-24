// code for SCSI(6) commands support 

// function declarations
void ProcSCSI6(BYTE devIndex); 

void SCSI_RequestSense(BYTE devIndex);
void SCSI_FormatUnit(BYTE devIndex);

void SCSI_ReadWrite6(BYTE devIndex, BYTE Read);

BYTE SCSI_Read6_SDMMC(BYTE devIndex, DWORD sector, WORD lenX);
BYTE SCSI_Write6_SDMMC(BYTE devIndex, DWORD sector, WORD lenX);

BYTE SCSI_Write6_USB(DWORD sector, WORD lenX);
BYTE SCSI_Read6_USB(DWORD sector, WORD lenX);

void SCSI_Inquiry(BYTE devIndex);

void SCSI_ModeSense6(BYTE devIndex);


void SendOKstatus(BYTE devIndex);
void ReturnStatusAccordingToIsInit(BYTE devIndex);
void ReturnUnitAttention(BYTE devIndex);
void ClearTheUnitAttention(BYTE devIndex);
void Return_LUNnotSupported(BYTE devIndex);

void SendEmptySecotrs(WORD sectors);

void showCommand(WORD id, WORD length, WORD errCode);

