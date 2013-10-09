// MANDATORY COMMANDS ARE:
// INQUIRY              x
// REQUEST SENSE        x
// SEND DIAGNOSTIC      x
// TEST UNIT READY      x 
// FORMAT UNIT          x
// READ(6)              x
// READ(10)             x              
// READ CAPACITY        x
// RELEASE              x
// RESERVE              x

// commands with length 6 bytes
#define SCSI_C_WRITE6                           0x0a
#define SCSI_C_READ6                            0x08
#define SCSI_C_MODE_SENSE6                      0x1a
#define SCSI_C_START_STOP_UNIT                  0x1b	
#define SCSI_C_FORMAT_UNIT                      0x04	
#define SCSI_C_INQUIRY                          0x12	
#define SCSI_C_REQUEST_SENSE                    0x03	
#define SCSI_C_TEST_UNIT_READY                  0x00	
#define SCSI_C_SEND_DIAGNOSTIC                  0x1d
#define SCSI_C_RESERVE                          0x16
#define SCSI_C_RELEASE                          0x17

// commands with length 10 bytes
#define SCSI_C_WRITE10                          0x2a
#define SCSI_C_READ10                           0x28
#define SCSI_C_WRITE_LONG                       0x3f
#define SCSI_C_READ_LONG                        0x3e
#define SCSI_C_READ_CAPACITY                    0x25
#define SCSI_C_VERIFY                           0x2f

// status bytes
#define SCSI_ST_OK                              0x00
#define SCSI_ST_CHECK_CONDITION                 0x02
#define SCSI_ST_CONDITION_MET                   0x04
#define SCSI_ST_BUSY                            0x08
#define SCSI_ST_INTERMEDIATE                    0x10
#define SCSI_ST_INTERM_COND_MET                 0x14
#define SCSI_ST_RESERV_CONFLICT                 0x18
#define SCSI_ST_COMMAND_TERM                    0x22
#define SCSI_ST_QUEUE_FULL                      0x28

// errors
#define SCSI_E_NoSense                          0x00
#define SCSI_E_RecoveredError                   0x01
#define SCSI_E_NotReady                         0x02
#define SCSI_E_MediumError                      0x03
#define SCSI_E_HardwareError                    0x04
#define SCSI_E_IllegalRequest                   0x05
#define SCSI_E_UnitAttention                    0x06
#define SCSI_E_DataProtect                      0x07
#define SCSI_E_BlankCheck                       0x08
#define SCSI_E_VendorSpecific                   0x09
#define SCSI_E_CopyAborted                      0x0a
#define SCSI_E_AbortedCommand                   0x0b
#define SCSI_E_Equal                            0x0c
#define SCSI_E_VolumeOverflow                   0x0d
#define SCSI_E_Miscompare                       0x0e
//------------------------------------------------------
// SCSI ADITIONAL SENSE CODES
#define SCSI_ASC_NO_ADDITIONAL_SENSE            0x00
#define SCSI_ASC_LU_NOT_READY                   0x04
#define SCSI_ASC_LU_NOT_SUPPORTED               0x25
#define SCSO_ASC_INVALID_FIELD_IN_CDB           0x24
#define SCSI_ASC_VERIFY_MISCOMPARE              0x1d
#define SCSI_ASC_MEDIUM_NOT_PRESENT             0x3A     

// SCSI ADITIONAL SENSE CODE QUALIFIER
#define SCSI_ASCQ_FORMAT_IN_PROGRESS            0x04
#define SCSI_ASCQ_NO_ADDITIONAL_SENSE           0x00

// ASC & ASCQ
#define SCSI_ASC_InvalidFieldInCDB              0x24
#define SCSI_ASC_InvalidCommandOperationCode    0x20

#define SCSI_ASC_INQUIRY_data_has_changed       0x3f
#define SCSI_ASCQ_INQUIRY_data_has_changed      0x03

#define SCSI_ASC_NOT_READY_TO_READY_TRANSITION  0x28

