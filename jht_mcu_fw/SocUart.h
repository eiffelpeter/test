#ifndef __SOCUART__
#define __SOCUART__

#ifdef __cplusplus
 extern "C" {
#endif

#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"
#include "string.h"


/* Private variables ---------------------------------------------------------*/
typedef char			CHAR;			//1 byte
typedef short			SHORT;		//2 bytes
typedef int				INT;			//4 bytes
typedef long			LONG;			//4 bytes
typedef long long	LONGLONG;

typedef unsigned char				BOOL;			//1 byte
typedef	unsigned char				uCHAR;		//1 byte
typedef	unsigned short			uSHORT;		//2 bytes
typedef unsigned int				uINT;			//4 bytes
typedef	unsigned long				uLONG;		//4 bytes
typedef unsigned long long	uLONGLONG;
typedef	double							DOUBLE;		//8 bytes


typedef void (*Fun)(void);
typedef void (**pFun[])(uCHAR, uCHAR *);


#pragma pack(1)

typedef enum _SOCUARTCMD_
{
	// Control class command table(0x00XX)
	Tone = 0x0001,
	Fan,
	Power,
	ERP,
	Debug,
	Reboot,
	MCU_Watchdog,
	QI = 0x0009,
	System_parameter,
	Write_Power_Monitor,
	Key,
	
	// Watchdog class command table(0x01XX)
	Watchdog = 0x0100,
	Refresh,
	
	// Event class command table(0x02XX)
	Key_Event = 0x0200,
	Safety_Key_Event,
	Debug_Message_Event,
	LCB_Status,
	Power_Monitor_Event,
	
	// Information/state class command table(0x03XX)
	Version = 0x0300,
	Safety_Key,
	Power_State,
	Boot,
	Read_LCB_Type,
	Recovery_State,
	Fan_Speed,
	System_State,
	Read_Power_Monitor_State,
	Key_State,
	Power_Monitor,
	
	// Firmware update class command table(0x04XX)
	Update_Parameter = 0x0400,
	Earse_Code,
	Write_Code,
	Read_Code,
	Start_Update,
	End_Update,
	Operation_Mode,
	Application_Name,
	Program_State,
	
}_SOCUARTCMD;

typedef enum _SOCCMDRESULT_
{
	SoCSuccess = 0x0000,
	SoCUnknownCommand,
	SoCInvalidPayloadLength,
	SoCInvalidParameters,
	SoCInvalidAddress,
	SoCInvalidPassword,
	SoCFailure,	
}_SOCCMDRESULT;


typedef struct _SOCCMD_
{
	uSHORT		DtaLen;				// Payload data length
	uSHORT		Cmd;					// 0 : Read command. 1 : Write command.
	uCHAR			CRC8;
	uCHAR			*pDta;
}_SOCCMD,*_pSOCCMD;

typedef struct _SOCCMDFMT_
{
	uSHORT	DtaCnt;
	uCHAR		StartStatus;	//Stop = 0, Start = 1
	uSHORT	Cmd;
	uSHORT	Reserved;
	uSHORT	DtaLen;
	uCHAR		*pDta;
	uCHAR		CRC8;
}_SOCCMDFMT,*_pSOCCMDFMT;

typedef struct _MCUCMDFMT_
{
	uSHORT	StartWord;
	uSHORT	Cmd;
	uSHORT	Reserved;
	uSHORT	DtaLen;
	uCHAR		pDta[];
//	uCHAR		CRC8;
}_MCUCMDFMT,*_pMCUCMDFMT;

typedef struct _MCURPYFMT_
{
	uSHORT	StartWord;
	uSHORT	Cmd;
	uSHORT	Result;
	uSHORT	DtaLen;
	uCHAR		pDta[];
//	uCHAR		CRC8;
}_MCURPYFMT,*_pMCURPYFMT;

typedef struct _MCUFWUPDATE_
{
	uCHAR	UnLock;						//Unlock the Bootload and Jump to bootload by StartUpdate command.
	uCHAR	EraseCmdReady;		//Memory Flash Erase command ready!
	uCHAR	EndUpdateCmdReady;//Lock the Bootload and Jump to app by EndUpdate command.
	uCHAR	ProgramDone;			//Program state is success or fail!
}_MCUFWUPDATE, *_pMCUFWUPDATE;

typedef struct _FLASHPAR_
{
	uLONG	ApStartAddr;
	uLONG	ApEndAddr;
	uLONG	PageSize;
	uCHAR	MaxPagesPerpacket;
	uLONG	NVRamStartAddr;
	uLONG	NVRamEndAddr;
}_FLASHPAR, *_pFLASHPAR;

typedef struct _READWRTIECTL_
{
	uINT	StartAddr;
	uINT	Length;
	uCHAR pDta[];
}_READWRTIECTL, *_pREADWRTIECTL;

#pragma pack()
extern uCHAR pSoCUSART2Dta[];
extern uCHAR SocCmdBuf[];
extern uCHAR HostCmdWaitReply;


void SoCUartReceiveCtrl(void);
void SocUartCmdHandle(uCHAR *pDta);
void SocUartCmdProcess(int len);
void SendSoCEventCmdReq(uSHORT CmdType, uCHAR Len, uCHAR *pDta);
//void SoCUartGpioStart(void);
//void SoCUartGpioStop(void);
void SendHostSocCmd(_SOCUARTCMD CmdType, uSHORT DtaLen, uCHAR *pDta);

#ifdef __cplusplus
}
#endif

#endif

