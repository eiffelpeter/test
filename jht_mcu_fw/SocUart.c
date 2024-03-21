#include "SocUart.h"
#include "MemoryFlashWriteRead.h"

extern int chimera_uart_puts(const char *msg, int len);
extern int chimera_uart_gets(char *msg, int len);

uCHAR SocCmdBuf[0x230];

_SOCCMDFMT 	SocCmd = {0};
_pSOCCMDFMT	pSocCmd = (_pSOCCMDFMT)(&SocCmd);

_MCUFWUPDATE McuFwUpdateCtl = {0};
_pMCUFWUPDATE pMcuFwUpdateCtl = (_pMCUFWUPDATE)(&McuFwUpdateCtl);

_MEMINF MemInf = 
{
	APP_FLASH_PARTITION,
	APP_BACK_UP_FLASH_PARTITION - 1,
	0x20000,
	1,
	INFO_FLASH_PARTITION + 0x2000,
	INFO_BACK_UP_FLASH_PARTITION - 1,
};
_pMEMINF pMemInf = (_pMEMINF)(&MemInf);

uCHAR pSoCUSART2Dta[0x230] = {0};


//#define	DebugSoC
#ifdef DebugSoC
	#define DebugSocPrintf printf
#else
	#define DebugSocPrintf(...) 
#endif

void SocUartCmdProcess(int len);

void SoCUartReceiveCtrl(void)
{
	int len = 0, retry = 10;

	memset(pSoCUSART2Dta, 0, sizeof(pSoCUSART2Dta));
	memset(SocCmdBuf, 0, sizeof(SocCmdBuf));

	do {
		len = chimera_uart_gets((char *)pSoCUSART2Dta, sizeof(pSoCUSART2Dta));
		retry--;
	} while ((len <= 0) && retry);

	if (len) {
		SocUartCmdHandle(pSoCUSART2Dta);
		//pSoCUSART2Dta[0] = 0;
		SocUartCmdProcess(len);
	}
}
static uCHAR Soc_CRC8_Table[16] =
						{ 0x00, 0x5A, 0xB4, 0xEE, 0x32, 0x68, 0x86, 0xDC, 0x64, 0x3E,
							0xD0, 0x8A, 0x56, 0x0C, 0xE2, 0xB8 };
static uCHAR Lcb_CRC8_Table[16] =
						{ 0x00, 0x31, 0x62, 0x53, 0xC4, 0xF5, 0xA6, 0x97, 0xB9, 0x88,
							0xDB, 0xEA, 0x7D, 0x4C, 0x1F, 0x2E };
						
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
//*****************************************************************************
//* Function Name  : DEV_RS485_Get_CRC8
//* Description    :
//* Input          : None
//* Output         : None
//* Return         : None
//*****************************************************************************
uCHAR	Get_CRC8(uCHAR *Ptr, uCHAR DtaLen,uCHAR CrcSelect)
{
	uCHAR		CRC_Count;
	uCHAR		CRC_Temp, CRC_Half, CRC_Data = 0;
	uCHAR		*pCrcTable;	
	
	if(CrcSelect == 0)
		pCrcTable = Soc_CRC8_Table;
	else
		pCrcTable = Lcb_CRC8_Table;
	
	for (CRC_Count = 0; CRC_Count < DtaLen; CRC_Count++)
	{
		CRC_Temp   = *(Ptr + CRC_Count);
		CRC_Half   = (CRC_Data / 16);
		CRC_Data <<= 4;
		CRC_Data  ^= pCrcTable[ CRC_Half ^ (CRC_Temp / 16)];
		CRC_Half   = (CRC_Data / 16);
		CRC_Data <<= 4;
		CRC_Data  ^= pCrcTable[ CRC_Half ^ (CRC_Temp & 0x0F)];
	}
	return  CRC_Data;
}

void SocUartCmdHandle(uCHAR *pDta)
{
	uSHORT	len = 0;

	if((*pDta == 0x55) && (*(pDta+1) == 0xAA))
	{
		SocCmdBuf[0] = *(pDta+6)+9; //(uCHAR)((pSocCmd->DtaLen+9) & 0x00ff);
		SocCmdBuf[1] = *(pDta+7)+9; //(uCHAR)(((pSocCmd->DtaLen+9) & 0xff00)>>8);
		SocCmdBuf[2] = 0x55;
		SocCmdBuf[3] = 0xAA;
		SocCmdBuf[4] = *(pDta+2); //(uCHAR)(pSocCmd->Cmd & 0x00ff);
		SocCmdBuf[5] = *(pDta+3); //(uCHAR)((pSocCmd->Cmd & 0xff00)>>8);
		SocCmdBuf[6] = 0x00;
		SocCmdBuf[7] = 0x00;
		SocCmdBuf[8] = *(pDta+6); //(uCHAR)(pSocCmd->DtaLen & 0x00ff);
		SocCmdBuf[9] = *(pDta+7); //(uCHAR)((pSocCmd->DtaLen & 0xff00)>>8);
	
		len = (*(pDta+7) << 8) | *(pDta+6);
		if (len)
			memcpy((SocCmdBuf+10), pDta+8, len);
		
		SocCmdBuf[10+len] = *(pDta+len+8);  // CRC8
#if DEBUG
		printf("header 0x%02X 0x%02X\r\n", *pDta, *(pDta+1));
		printf("cmd 0x%02X 0x%02X\r\n", *(pDta+3), *(pDta+2));
		printf("payload len %d\r\n", len);
		printf("crc8 0x%02X\r\n", *(pDta+len+8));

		if (len)
		{
			for (int i=0; i<len ; i++) {
				printf("0x%02X ", SocCmdBuf[10+i]);
			}
			printf("\r\n");
		}
#endif
	} else {
		printf("header is not 0x55 0xAA at %s\r\n", __func__);
	}
}


//void SoCUartGpioStart(void)
//{
//	GPIO_InitTypeDef GPIO_InitStruct = {0};
//	
//	/* Peripheral clock enable */
//	__HAL_RCC_USART1_CLK_ENABLE();
//	USART1_TX_GPIO_CLK_ENABLE();
//	USART1_RX_GPIO_CLK_ENABLE();
//	/**USART1 GPIO Configuration
//		PA9      ------> USART1_TX
//		PA10     ------> USART1_RX
//	*/
//	GPIO_InitStruct.Pin = USART1_TX_PIN|USART1_RX_PIN;
//	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
//	GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
//	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
//	
//	memset(((uCHAR *)pSocCmd), 0, sizeof(_SOCCMDFMT));
//	memset(SocCmdBuf, 0, 0x230);
//}

//void SoCUartGpioStop(void)
//{
//	HAL_GPIO_DeInit(GPIOA, USART1_TX_PIN|USART1_RX_PIN);
//}

void SocUartSendCmd(uCHAR *pDta)
{
	_pMCUCMDFMT pSendBuf = (_pMCUCMDFMT)pDta;
	*(pSendBuf->pDta + pSendBuf->DtaLen) = Get_CRC8(pDta,pSendBuf->DtaLen+8, 0);
//	printf(" Send Buf Dta :");
//	for(uINT Cnt=0; Cnt<(pSendBuf->DtaLen+9); Cnt++)
//		printf(" 0x%02X", pDta[Cnt]);
//	printf("\r\n\r\n");
// 	while(HAL_UART_Transmit(&Uart2Handle,pDta,(pSendBuf->DtaLen+9),100) != HAL_OK);
   chimera_uart_puts((char *)pDta,(pSendBuf->DtaLen+9));
}

static void FiexdFileParSet(_SOCUARTCMD SetType, uSHORT DtaLen,uCHAR *pDtaAddr)
{
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)pDtaAddr;
	
	pRespBuf->StartWord = 0xAA55;
	pRespBuf->Cmd = SetType;
	pRespBuf->Reserved = 0x0000;
	pRespBuf->DtaLen = DtaLen;
}


#ifdef	APP_RUN
static void SoCNULLCmd(uCHAR Len, uCHAR *pDta)
{
	printf("Receive Not supports command from SOC!\r\n\r\n");
}

static void SetTone(uCHAR Len, uCHAR *pDta)
{
//	uCHAR *RespBuf = (uCHAR *)malloc(9);
	uCHAR RespBuf[9];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
//	printf("In %s func malloc Ptr : 0x%08X\r\n",__FUNCTION__, (uLONG)(RespBuf));
	BuzzerTrigerPrecess();
	pRespBuf->StartWord = 0xAA55;
	pRespBuf->Cmd = Tone;
	pRespBuf->Reserved = 0x0000;
	pRespBuf->DtaLen = 0x0000;
	SocUartSendCmd(RespBuf);
//	printf("In %s func free Ptr : 0x%08X\r\n",__FUNCTION__, (uLONG)(RespBuf));
//	free(RespBuf);
}

static void SetFanSpeed(uCHAR Len, uCHAR *pDta)
{
//	uCHAR *RespBuf = (uCHAR *)malloc(9);
	uCHAR RespBuf[9];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
	
	if(pDta[0] <= 9)
		PwmDuty = 0;
	else
		PwmDuty = pDta[0];
	
	DebugSocPrintf("SetFan Dta Len : 0x%02X",Len);
	DebugSocPrintf("Set Fan : 0x%02X\r\n",pDta[0]);
	
	pRespBuf->StartWord = 0xAA55;
	pRespBuf->Cmd = Fan;
	pRespBuf->Reserved = 0x0000;
	pRespBuf->DtaLen = 0x0000;
	SocUartSendCmd(RespBuf);
//	free(RespBuf);
}

static void SetPowerEn(uCHAR Len, uCHAR *pDta)
{
	uCHAR RespBuf[9];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
	_pPWRSTATE pSetPwrDta = (_pPWRSTATE)pPwrState;
	
	if(pDta[1])
		pSetPwrDta[pDta[0]-1].En = 0x01;
	else
		pSetPwrDta[pDta[0]-1].En = 0x00;
	
	DebugSocPrintf("SetPowerEn 0x%02X : 0x%02X\r\n",pDta[0], pDta[1]);
	
	pRespBuf->StartWord = 0xAA55;
	pRespBuf->Cmd = Power;
	pRespBuf->Reserved = 0x0000;
	pRespBuf->DtaLen = 0x0000;
	SocUartSendCmd(RespBuf);
}
static void ERPCmd(uCHAR Len, uCHAR *pDta)
{
//	uCHAR *RespBuf = (uCHAR *)malloc(9);
	uCHAR RespBuf[9];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
	
	pRespBuf->StartWord = 0xAA55;
	pRespBuf->Cmd = ERP;
	pRespBuf->Reserved = 0x0000;
	pRespBuf->DtaLen = 0x0000;
	SocUartSendCmd(RespBuf);
	
	PowerERPProcess(Into_ERP_Mode);
//	free(RespBuf);
}

static void DebugEn(uCHAR Len, uCHAR *pDta)
{
//	uCHAR *RespBuf = (uCHAR *)malloc(9);
	uCHAR RespBuf[9];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
	
	pRespBuf->StartWord = 0xAA55;
	pRespBuf->Cmd = Debug;
	pRespBuf->Reserved = 0x0000;
	pRespBuf->DtaLen = 0x0000;
	SocUartSendCmd(RespBuf);
//	free(RespBuf);
}

static void KeyEventResp(uCHAR Len, uCHAR *pDta)
{
	DebugSocPrintf("Into the %s function.\r\n",__FUNCTION__);
	DebugSocPrintf("Receive the Key event resp from SoC.\r\n");
}

static void SafetyKeyEventResp(uCHAR Len, uCHAR *pDta)
{
	DebugSocPrintf("Into the %s function.\r\n",__FUNCTION__);
	DebugSocPrintf("Receive the Safety Key event resp from SoC.\r\n");
}

void (*EventRespFun[])(uCHAR, uCHAR *) = 
{
	KeyEventResp,								//0x00 : Key event Resp
	SafetyKeyEventResp,					//0x01 : Safety key event Resp
	SoCNULLCmd,									//0x02 : Debug message event Resp
	SoCNULLCmd,									//0x03 : LCB status Resp
	SoCNULLCmd,									//0x04 : Power monitor event Resp
};

static void WatchDogCtrl(uCHAR Len, uCHAR *pDta)
{
//	uCHAR *RespBuf = (uCHAR *)malloc(9);
	uCHAR RespBuf[9];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
	
	pRespBuf->StartWord = 0xAA55;
	pRespBuf->Cmd = Watchdog;
	pRespBuf->Reserved = 0x0000;
	pRespBuf->DtaLen = 0x0000;
	SocUartSendCmd(RespBuf);
//	free(RespBuf);
}

static void WatchDogRefresh(uCHAR Len, uCHAR *pDta)
{
//	uCHAR *RespBuf = (uCHAR *)malloc(9);
	uCHAR RespBuf[9];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
	
	pRespBuf->StartWord = 0xAA55;
	pRespBuf->Cmd = Refresh;
	pRespBuf->Reserved = 0x0000;
	pRespBuf->DtaLen = 0x0000;
	SocUartSendCmd(RespBuf);
//	free(RespBuf);
}

void (*WatchDogCtrlReplyFun[])(uCHAR, uCHAR *) = 
{
	WatchDogCtrl,								//0x00 : Enable the MCU watchdog.
	WatchDogRefresh,						//0x01 : Watch Dog count Refresh.
};

static void SocKeyEvent(uCHAR Len, uCHAR *pDta)
{
//	uCHAR *RespBuf = (uCHAR *)malloc(Len+9);
	uCHAR RespBuf[Len+9];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
	
	pRespBuf->StartWord = 0xAA55;
	pRespBuf->Cmd = Key_Event;
	pRespBuf->Reserved = 0x0000;
	pRespBuf->DtaLen = Len;
	memcpy(pRespBuf->pDta, pDta, Len);		//Copy the Matrix Key status
	SocUartSendCmd(RespBuf);
//	free(RespBuf);
}

static void SocSafetyKeyEvent(uCHAR Len, uCHAR *pDta)
{
//	uCHAR *RespBuf = (uCHAR *)malloc(Len+9);
	uCHAR RespBuf[Len+9];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
	
	pRespBuf->StartWord = 0xAA55;
	pRespBuf->Cmd = Safety_Key_Event;
	pRespBuf->Reserved = 0x0000;
	pRespBuf->DtaLen = Len;
	memcpy(pRespBuf->pDta, pDta, Len);		//Copy the Matrix Key status
	SocUartSendCmd(RespBuf);
//	free(RespBuf);
}

void SendSoCEventCmdReq(uSHORT CmdType, uCHAR Len, uCHAR *pDta)
{
	void (*SendSocEventCmdFun[])(uCHAR ,uCHAR *) = 
	{
		SocKeyEvent,
		SocSafetyKeyEvent,
		SoCNULLCmd,
		SoCNULLCmd,
		SoCNULLCmd,
	};
	if((CmdType&0x0200) == 0x0200)
	{
		uCHAR FuncCnt = (uCHAR)(CmdType&0x00FF);
		(*SendSocEventCmdFun[FuncCnt])(Len, pDta);
		return;
	}
	printf("The Send Soc Event command type Request is Error type!\r\n\r\n");
}


static void ReadMcuFwVersion(uCHAR Len, uCHAR *pDta)
{
	uSHORT ReturnLen = 2;
//	uCHAR *RespBuf = (uCHAR *)malloc(ReturnLen+9);
	uCHAR RespBuf[ReturnLen+9];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;

	pRespBuf->StartWord = 0xAA55;
	pRespBuf->Cmd = Version;
	pRespBuf->Reserved = 0x0000;
	pRespBuf->DtaLen = ReturnLen;
	
//	pRespBuf->pDta[0] = 0x02;
//	pRespBuf->pDta[1] = 0x00;
	pRespBuf->pDta[0] = (uCHAR)(MCU_Version&0x00FF);
	pRespBuf->pDta[1] = (uCHAR)((MCU_Version&0xFF00)>>8);

	SocUartSendCmd(RespBuf);
//	free(RespBuf);
}

static void GetSafetyKeyState(uCHAR Len, uCHAR *pDta)
{
	uSHORT ReturnLen = 1;
	uCHAR RespBuf[ReturnLen+9];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
	
	pRespBuf->StartWord = 0xAA55;
	pRespBuf->Cmd = Safety_Key;
	pRespBuf->Reserved = 0x0000;
	pRespBuf->DtaLen = ReturnLen;
	
	if(HAL_GPIO_ReadPin(DET_SOC_Port, DET_SOC_Pin) == 1)
		pRespBuf->pDta[0] = 1;
	else
		pRespBuf->pDta[0] = 0;
	
	SocUartSendCmd(RespBuf);
}

static void ReadPowerState(uCHAR Len, uCHAR *pDta)
{
	uSHORT ReturnLen = 5;
//	uCHAR *RespBuf = (uCHAR *)malloc(ReturnLen+9);
	uCHAR RespBuf[ReturnLen+9];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
	_pPWRSTATE pReturnDta;
	
	pRespBuf->StartWord = 0xAA55;
	pRespBuf->Cmd = Power_State;
	pRespBuf->Reserved = 0x0000;
	pRespBuf->DtaLen = ReturnLen;
	
	DebugSocPrintf("Read Power status 0x%02X \r\n",pDta[0]);
	
	pReturnDta = (_pPWRSTATE)(&(pPwrState->RS485PwrState) + *pDta - 1);
	pRespBuf->pDta[0] = *pDta;
	pRespBuf->pDta[1] = pReturnDta->State;
	pRespBuf->pDta[2] = 0x00;
	pRespBuf->pDta[3] = (uCHAR)(pReturnDta->Current & 0xFF);
	pRespBuf->pDta[4] = (uCHAR)((pReturnDta->Current & 0xff00) >> 8);
//	memcpy(pRespBuf->pDta, (uCHAR *)(&(pPwrState->RS485PwrState) + *pDta - 1), sizeof(_PWRSTATE));
	
	SocUartSendCmd(RespBuf);
//	free(RespBuf);
}

static void MCUReBootState(uCHAR Len, uCHAR *pDta)
{
	uSHORT ReturnLen = 1;
//	uCHAR *RespBuf = (uCHAR *)malloc(ReturnLen+9);
	uCHAR RespBuf[ReturnLen+9];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
	
	pRespBuf->StartWord = 0xAA55;
	pRespBuf->Cmd = Boot;
	pRespBuf->Reserved = 0x0000;
	pRespBuf->DtaLen = ReturnLen;
	
	pRespBuf->pDta[0] = 0x00;
	
	SocUartSendCmd(RespBuf);
//	free(RespBuf);
}

static void ReadLCBType(uCHAR Len, uCHAR *pDta)
{
	uSHORT ReturnLen = 1;
//	uCHAR *RespBuf = (uCHAR *)malloc(ReturnLen+9);
	uCHAR RespBuf[ReturnLen+9];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
	
	pRespBuf->StartWord = 0xAA55;
	pRespBuf->Cmd = Read_LCB_Type;
	pRespBuf->Reserved = 0x0000;
	pRespBuf->DtaLen = ReturnLen;
	
	pRespBuf->pDta[0] = 0xC2;
	
	SocUartSendCmd(RespBuf);
//	free(RespBuf);
}

static void ReadRecoveryKeyStatus(uCHAR Len, uCHAR *pDta)
{
	uSHORT ReturnLen = 1;
//	uCHAR *RespBuf = (uCHAR *)malloc(ReturnLen+9);
	uCHAR RespBuf[ReturnLen+9];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
	
	pRespBuf->StartWord = 0xAA55;
	pRespBuf->Cmd = Recovery_State;
	pRespBuf->Reserved = 0x0000;
	pRespBuf->DtaLen = ReturnLen;
	
//	if(HAL_GPIO_ReadPin(VDD_PHY3V3_PG_Port, VDD_PHY3V3_PG_Pin))
	if(HAL_GPIO_ReadPin(RECOVERY_Port, RECOVERY_Pin))
		pRespBuf->pDta[0] = 0x00;
//		pRespBuf->pDta[0] = 0x01;
	else
		pRespBuf->pDta[0] = 0x01;
//		pRespBuf->pDta[0] = 0x00;
	
	SocUartSendCmd(RespBuf);
//	free(RespBuf);
}

static void ReadFanSpeed(uCHAR Len, uCHAR *pDta)
{
	uSHORT ReturnLen = 3;
//	uCHAR *RespBuf = (uCHAR *)malloc(ReturnLen+9);
	uCHAR RespBuf[ReturnLen+9];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
	
	pRespBuf->StartWord = 0xAA55;
	pRespBuf->Cmd = Fan_Speed;
	pRespBuf->Reserved = 0x0000;
	pRespBuf->DtaLen = ReturnLen;
	
	pRespBuf->pDta[0] = PwmDuty;
	pRespBuf->pDta[1] = (uCHAR)(FanRPM & 0x00FF);
	pRespBuf->pDta[2] = (uCHAR)((FanRPM>>8) & 0x00FF);
	
	SocUartSendCmd(RespBuf);
//	free(RespBuf);
}

static void ReadSystemStatus(uCHAR Len, uCHAR *pDta)
{
	uSHORT ReturnLen;
	uCHAR *pDtabuf;
	switch(*pDta)
	{
		case 0:
			DebugSocPrintf("Return the Ethernet MAC address!\r\n");
			ReturnLen = 6;
			pDtabuf = malloc(ReturnLen);
			pDtabuf[0] = 0x00;
			pDtabuf[1] = 0x10;
			pDtabuf[2] = 0xF3;
			pDtabuf[3] = 0x00;
			pDtabuf[4] = 0x00;
			pDtabuf[5] = 0x01;
			break;
		case 1:
			DebugSocPrintf("Return the Serial No (SOM)!\r\n");
			ReturnLen = 12;
			pDtabuf = malloc(ReturnLen);
			pDtabuf[0] = 0x31;
			pDtabuf[1] = 0x32;
			pDtabuf[2] = 0x33;
			pDtabuf[3] = 0x34;
			pDtabuf[4] = 0x35;
			pDtabuf[5] = 0x36;
			pDtabuf[6] = 0x37;
			pDtabuf[7] = 0x38;
			pDtabuf[8] = 0x39;
			pDtabuf[9] = 0x30;
			pDtabuf[10] = 0x31;
			pDtabuf[11] = 0x32;
			break;
		case 2:
			DebugSocPrintf("Return the ASSY (SOM)!\r\n");
			ReturnLen = 4;
			pDtabuf = malloc(ReturnLen);
			pDtabuf[0] = 0x41;
			pDtabuf[1] = 0x42;
			pDtabuf[2] = 0x43;
			pDtabuf[3] = 0x44;
			break;
		case 3:
			DebugSocPrintf("Return the Serial No1 (CA)!\r\n");
			ReturnLen = 12;
			pDtabuf = malloc(ReturnLen);
			pDtabuf[0] = 0x31;
			pDtabuf[1] = 0x32;
			pDtabuf[2] = 0x33;
			pDtabuf[3] = 0x34;
			pDtabuf[4] = 0x35;
			pDtabuf[5] = 0x36;
			pDtabuf[6] = 0x37;
			pDtabuf[7] = 0x38;
			pDtabuf[8] = 0x39;
			pDtabuf[9] = 0x30;
			pDtabuf[10] = 0x31;
			pDtabuf[11] = 0x32;
			break;
		case 4:
			DebugSocPrintf("Return the ASSY1 (CA)!\r\n");
			ReturnLen = 4;
			pDtabuf = malloc(ReturnLen);
			pDtabuf[0] = 0x41;
			pDtabuf[1] = 0x42;
			pDtabuf[2] = 0x43;
			pDtabuf[3] = 0x44;
			break;
		case 5:
			DebugSocPrintf("Return the Serial No2 (SYSTEM)!\r\n");
			ReturnLen = 12;
			pDtabuf = malloc(ReturnLen);
			pDtabuf[0] = 0x31;
			pDtabuf[1] = 0x32;
			pDtabuf[2] = 0x33;
			pDtabuf[3] = 0x34;
			pDtabuf[4] = 0x35;
			pDtabuf[5] = 0x36;
			pDtabuf[6] = 0x37;
			pDtabuf[7] = 0x38;
			pDtabuf[8] = 0x39;
			pDtabuf[9] = 0x30;
			pDtabuf[10] = 0x31;
			pDtabuf[11] = 0x32;
			break;
		default:
			return;
	}
	{
		uCHAR *SendBuf = (uCHAR *)malloc(ReturnLen+9);
		_pMCUCMDFMT pSendBuf = (_pMCUCMDFMT)SendBuf;
		
		pSendBuf->StartWord = 0xAA55;			//Because the uSHORT type will reversed the high/low byte
		pSendBuf->Cmd = System_State;
		pSendBuf->Reserved = 0;
		pSendBuf->DtaLen = ReturnLen;
		memcpy(pSendBuf->pDta, pDtabuf, ReturnLen);
		
		SocUartSendCmd(SendBuf);
		free(SendBuf);
	}
	free(pDtabuf);
}


void (*CtrlReplyFun[])(uCHAR, uCHAR *) = 
{
	SoCNULLCmd,									//0x00 : SoCNULLCmd
	SetTone,							//0x01 : Tone : play a tone from the sound chip.
	SetFanSpeed,					//0x02 : Fan : Set fan speed.
	SetPowerEn,						//0x03 : Power : Enable / disable various power source
	ERPCmd,								//0x04 : ERP : Send this command to notification MCU to enter ERP mode
	DebugEn,							//0x05 : Debug : Enable/disable I/O debug messages to be sent to the SOC.
	SoCNULLCmd,									//0x06 : SoCNULLCmd
	SoCNULLCmd,									//0x07 : SoCNULLCmd
	SoCNULLCmd,									//0x08 : SoCNULLCmd
	SoCNULLCmd,									//0x09 : SoCNULLCmd
	SoCNULLCmd,									//0x0A : SoCNULLCmd
	SoCNULLCmd,									//0x0B : SoCNULLCmd
	SoCNULLCmd,									//0x0C : SoCNULLCmd
	SoCNULLCmd,									//0x0D : Unkoen SoC command
};

void (*MCUInfoReplyFun[])(uCHAR, uCHAR *) = 
{
	ReadMcuFwVersion,			//0x00 : Version : MCU firmware version
	GetSafetyKeyState,		//0x01 : Safety key : Get safety key state
	ReadPowerState,				//0x02 : Power state : Get the state of a particular power source
	MCUReBootState,				//0x03 : Boot : Check to see if MCU reboot MCU
	ReadLCBType,					//0x04 : LCB Type : Get the LCB type
	ReadRecoveryKeyStatus,//0x05 : Recovery State : This is used by OS to process recovery
	ReadFanSpeed,					//0x06 : Fan Speed : Get the Fan level and rpm
	ReadSystemStatus,			//0x07 : System state : Read system state
	SoCNULLCmd,						//0x08 : Power monitor state : Get the power monitor state
	SoCNULLCmd,						//0x09 : Key state : Get key state
	SoCNULLCmd,						//0x0A : Power monitor : Power monitor
};
#endif

static void UpdateParameter(uCHAR Len, uCHAR *pDta)
{
	uCHAR RespBuf[9 + sizeof(_MEMINF)];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
	printf("Into the %s fun!\r\n",__FUNCTION__);
	
	FiexdFileParSet(Update_Parameter, sizeof(_MEMINF), RespBuf);
	memcpy(pRespBuf->pDta, (uCHAR *)pMemInf, sizeof(_MEMINF));
	SocUartSendCmd(RespBuf);
}

static void EarseCode(uCHAR Len, uCHAR *pDta)
{
	uCHAR RespBuf[9];
	printf("Into the %s fun!\r\n",__FUNCTION__);
	
	FiexdFileParSet(Earse_Code, 0, RespBuf);
	
	pMcuFwUpdateCtl->EraseCmdReady = 1;
	
	SocUartSendCmd(RespBuf);
}

static void WriteCode(uCHAR Len, uCHAR *pDta)
{
	uCHAR RespBuf[9];
	uCHAR *pDestAddr;
	_pREADWRTIECTL pRWCtl = (_pREADWRTIECTL)pDta;
	
	printf("Into the %s fun!\r\n",__FUNCTION__);
	FiexdFileParSet(Write_Code, 0, RespBuf);
	pDestAddr = (uCHAR *)(SRAM1_ADDR_PARTITION + 
											 (((uLONG)(pRWCtl->StartAddr))&(~APP_FLASH_PARTITION)));
	
	memcpy(pDestAddr, pRWCtl->pDta, pRWCtl->Length);
	SocUartSendCmd(RespBuf);
}

static void ReadCode(uCHAR Len, uCHAR *pDta)
{
	uCHAR RespBuf[9+512];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
	uCHAR *pSourAddr;
	_pREADWRTIECTL pRWCtl = (_pREADWRTIECTL)pDta;
	
	printf("Into the %s fun!\r\n",__FUNCTION__);
	pSourAddr = (uCHAR *)(SRAM1_ADDR_PARTITION + 
											 (((uLONG)(pRWCtl->StartAddr))&(~APP_FLASH_PARTITION)));
	memcpy(pRespBuf->pDta, &(pRWCtl->StartAddr),pRWCtl->Length);
	memcpy((pRespBuf->pDta + 4), pSourAddr, pRWCtl->Length);
	
	FiexdFileParSet(Read_Code, 4+pRWCtl->Length, RespBuf);
	SocUartSendCmd(RespBuf);
}

static void StartUpdate(uCHAR Len, uCHAR *pDta)
{
	uCHAR RespBuf[9];
	
	printf("Into the %s fun!\r\n",__FUNCTION__);
	FiexdFileParSet(Start_Update, 0, RespBuf);
	
	if(memcmp(pDta, "johnson", Len) != 0)
		printf("The unlock password is error!\r\n");
	else
		pMcuFwUpdateCtl->UnLock = 1;
	
	SocUartSendCmd(RespBuf);
}

static void EndUpdate(uCHAR Len, uCHAR *pDta)
{
	uCHAR RespBuf[9];
	
	printf("Into the %s fun!\r\n",__FUNCTION__);
	FiexdFileParSet(End_Update, 0, RespBuf);
	
	pMcuFwUpdateCtl->EndUpdateCmdReady = 1;
	
	SocUartSendCmd(RespBuf);
}

static void ReadOperationMode(uCHAR Len, uCHAR *pDta)
{
	uCHAR RespBuf[10];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
	
	printf("Into the %s fun!\r\n",__FUNCTION__);
	
	FiexdFileParSet(Operation_Mode, 1, RespBuf);
#ifdef APP_RUN
	pRespBuf->pDta[0] = 0x00;		//0x00: Normal Mode
#else
	pRespBuf->pDta[0] = 0x01;		//0x01: Update Mode
#endif
	SocUartSendCmd(RespBuf);
}

static void ReadApplicationName(uCHAR Len, uCHAR *pDta)
{
	uCHAR RespBuf[17];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
	
	printf("Into the %s fun!\r\n",__FUNCTION__);
	
	FiexdFileParSet(Application_Name, 8, RespBuf);
	memcpy(pRespBuf->pDta, "CA18_R08", 8);
	
	SocUartSendCmd(RespBuf);
}

static void ProgramState(uCHAR Len, uCHAR *pDta)
{
	uCHAR RespBuf[10];
	_pMCUCMDFMT pRespBuf = (_pMCUCMDFMT)RespBuf;
	
	FiexdFileParSet(Program_State, 1, RespBuf);
	pRespBuf->pDta[0] = pMcuFwUpdateCtl->ProgramDone;	//0x00: AP has not been updated yet.
																										//0x01: AP have been updated.
	SocUartSendCmd(RespBuf);
}

void (*FirmwareUpdateReplyFun[])(uCHAR, uCHAR *) = 
{
	UpdateParameter,							//0x00 : Update Parameter
	EarseCode,										//0x01 : Earse Code
	WriteCode,										//0x02 : Write Code
	ReadCode,											//0x03 : Read Code
	StartUpdate,									//0x04 : Start Update
	EndUpdate,										//0x05 : End Update
	ReadOperationMode,						//0x06 : Read Operation mode
	ReadApplicationName,					//0x07 : Application Name
	ProgramState,									//0x08 : Program state
};


pFun SoCCmdReplyFun = 
{
#ifdef	APP_RUN
	CtrlReplyFun,
	WatchDogCtrlReplyFun,
	EventRespFun,
	MCUInfoReplyFun,
#else
	NULL,
	NULL,
	NULL,
	NULL,
#endif
	FirmwareUpdateReplyFun,
	NULL,
	NULL,
};


static void UpdateParameterReply(uCHAR Len, uCHAR *pDta)
{
//  _pMCURPYFMT pReplyDta = (_pMCURPYFMT)pDta;
//  _pFLASHPAR pFlashPar = (_pFLASHPAR)(pReplyDta->pDta);
	
//	printf("Into the %s fun!\r\n",__FUNCTION__);
//	
//	printf("pReplyDta->StartWord : 0x%04X\r\n",pReplyDta->StartWord );
//	printf("pReplyDta->Cmd : 0x%04X\r\n",pReplyDta->Cmd );
//	printf("pReplyDta->Result : 0x%04X\r\n",pReplyDta->Result );
//	printf("pReplyDta->DtaLen : 0x%04X\r\n",pReplyDta->DtaLen );
//	
//	
//	printf("ApStartAddr : 0x%08X\r\n", (uINT)(pFlashPar->ApStartAddr));
//	printf("ApEndAddr : 0x%08X\r\n", (uINT)(pFlashPar->ApEndAddr));
//	printf("PageSize : 0x%08X\r\n", (uINT)(pFlashPar->PageSize));
//	printf("MaxPagesPerpacket : 0x%02X\r\n", pFlashPar->MaxPagesPerpacket);
//	printf("NVRamStartAddr : 0x%08X\r\n", (uINT)(pFlashPar->NVRamStartAddr));
//	printf("NVRamEndAddr : 0x%08X\r\n", (uINT)(pFlashPar->NVRamEndAddr));
}

static void EarseCodeReply(uCHAR Len, uCHAR *pDta)
{
//  _pMCURPYFMT pReplyDta = (_pMCURPYFMT)pDta;
	
	printf("Into the %s fun!\r\n",__FUNCTION__);
	
//	printf("pReplyDta->StartWord : 0x%04X\r\n",pReplyDta->StartWord );
//	printf("pReplyDta->Cmd : 0x%04X\r\n",pReplyDta->Cmd );
//	printf("pReplyDta->Result : 0x%04X\r\n",pReplyDta->Result );
//	printf("pReplyDta->DtaLen : 0x%04X\r\n",pReplyDta->DtaLen );
//	if(pReplyDta->DtaLen != 0)
//	{
//		printf("Dta :");
//		for(uINT Cnt = 0; Cnt < pReplyDta->DtaLen; Cnt++)
//			printf(" 0x%02X",*(pReplyDta->pDta + Cnt));
//		printf("\r\n\r\n");
//	}
//	else
//		printf("\r\n");
}

static void WriteCodeReply(uCHAR Len, uCHAR *pDta)
{
//	_pMCURPYFMT pReplyDta = (_pMCURPYFMT)pDta;
	
//	printf("Into the %s fun!\r\n",__FUNCTION__);
//	
//	printf("pReplyDta->StartWord : 0x%04X\r\n",pReplyDta->StartWord );
//	printf("pReplyDta->Cmd : 0x%04X\r\n",pReplyDta->Cmd );
//	printf("pReplyDta->Result : 0x%04X\r\n",pReplyDta->Result );
//	printf("pReplyDta->DtaLen : 0x%04X\r\n",pReplyDta->DtaLen );
//	if(pReplyDta->DtaLen != 0)
//	{
//		printf("Dta :");
//		for(uINT Cnt = 0; Cnt < pReplyDta->DtaLen; Cnt++)
//			printf(" 0x%02X",*(pReplyDta->pDta + Cnt));
//		printf("\r\n\r\n");
//	}
//	else
//		printf("\r\n");
}

static void ReadCodeReply(uCHAR Len, uCHAR *pDta)
{
	extern uCHAR ReadBinTemp[];
	_pMCURPYFMT pReplyDta = (_pMCURPYFMT)pDta;
	
//	printf("Into the %s fun! DtaLen:%d\r\n",__FUNCTION__, pReplyDta->DtaLen);
	
//	printf("pReplyDta->StartWord : 0x%04X\r\n",pReplyDta->StartWord );
//	printf("pReplyDta->Cmd : 0x%04X\r\n",pReplyDta->Cmd );
//	printf("pReplyDta->Result : 0x%04X\r\n",pReplyDta->Result );
//	printf("pReplyDta->DtaLen : 0x%04X\r\n",pReplyDta->DtaLen );
	if(pReplyDta->DtaLen != 0)
		memcpy(ReadBinTemp, pReplyDta->pDta, pReplyDta->DtaLen);
//	if(pReplyDta->DtaLen != 0)
//	{
//		memcpy(ReadBinTemp, pReplyDta->pDta, pReplyDta->DtaLen);
//		printf("Dta :");
//		for(uINT Cnt = 0; Cnt < pReplyDta->DtaLen; Cnt++)
//			printf(" 0x%02X",*(pReplyDta->pDta + Cnt));
//		printf("\r\n\r\n");
//	}
//	else
//		printf("\r\n");
}

static void StartUpdateReply(uCHAR Len, uCHAR *pDta)
{
//	_pMCURPYFMT pReplyDta = (_pMCURPYFMT)pDta;
	
	printf("Into the %s fun!\r\n",__FUNCTION__);
	
//	printf("pReplyDta->StartWord : 0x%04X\r\n",pReplyDta->StartWord );
//	printf("pReplyDta->Cmd : 0x%04X\r\n",pReplyDta->Cmd );
//	printf("pReplyDta->Result : 0x%04X\r\n",pReplyDta->Result );
//	printf("pReplyDta->DtaLen : 0x%04X\r\n",pReplyDta->DtaLen );
//	if(pReplyDta->DtaLen != 0)
//	{
//		printf("Dta :");
//		for(uINT Cnt = 0; Cnt < pReplyDta->DtaLen; Cnt++)
//			printf(" 0x%02X",*(pReplyDta->pDta + Cnt));
//		printf("\r\n\r\n");
//	}
//	else
//		printf("\r\n");
}

static void EndUpdateReply(uCHAR Len, uCHAR *pDta)
{
//	_pMCURPYFMT pReplyDta = (_pMCURPYFMT)pDta;
	
	printf("Into the %s fun!\r\n",__FUNCTION__);
	
//	printf("pReplyDta->StartWord : 0x%04X\r\n",pReplyDta->StartWord );
//	printf("pReplyDta->Cmd : 0x%04X\r\n",pReplyDta->Cmd );
//	printf("pReplyDta->Result : 0x%04X\r\n",pReplyDta->Result );
//	printf("pReplyDta->DtaLen : 0x%04X\r\n",pReplyDta->DtaLen );
//	if(pReplyDta->DtaLen != 0)
//	{
//		printf("Dta :");
//		for(uINT Cnt = 0; Cnt < pReplyDta->DtaLen; Cnt++)
//			printf(" 0x%02X",*(pReplyDta->pDta + Cnt));
//		printf("\r\n\r\n");
//	}
//	else
//		printf("\r\n");
}

uCHAR McuCurrentMode = 0;
static void ReadOperationModeReply(uCHAR Len, uCHAR *pDta)
{
	_pMCURPYFMT pReplyDta = (_pMCURPYFMT)pDta;
	
	McuCurrentMode = pReplyDta->pDta[0];
	
	printf("Into the %s fun!\r\n",__FUNCTION__);
//	
//	printf("pReplyDta->StartWord : 0x%04X\r\n",pReplyDta->StartWord );
//	printf("pReplyDta->Cmd : 0x%04X\r\n",pReplyDta->Cmd );
//	printf("pReplyDta->Result : 0x%04X\r\n",pReplyDta->Result );
//	printf("pReplyDta->DtaLen : 0x%04X\r\n",pReplyDta->DtaLen );
//	if(pReplyDta->DtaLen != 0)
//	{
//		printf("Dta :");
//		for(uINT Cnt = 0; Cnt < pReplyDta->DtaLen; Cnt++)
//			printf(" 0x%02X",*(pReplyDta->pDta + Cnt));
//		printf("\r\n\r\n");
//	}
//	else
//		printf("\r\n");
}

static void ReadApplicationNameReply(uCHAR Len, uCHAR *pDta)
{
//	_pMCURPYFMT pReplyDta = (_pMCURPYFMT)pDta;
	
//	printf("Into the %s fun!\r\n",__FUNCTION__);
//	
//	printf("pReplyDta->StartWord : 0x%04X\r\n",pReplyDta->StartWord );
//	printf("pReplyDta->Cmd : 0x%04X\r\n",pReplyDta->Cmd );
//	printf("pReplyDta->Result : 0x%04X\r\n",pReplyDta->Result );
//	printf("pReplyDta->DtaLen : 0x%04X\r\n",pReplyDta->DtaLen );
//	if(pReplyDta->DtaLen != 0)
//	{
//		printf("Dta :");
//		for(uINT Cnt = 0; Cnt < pReplyDta->DtaLen; Cnt++)
//			printf(" 0x%02X",*(pReplyDta->pDta + Cnt));
//		printf("\r\n\r\n");
//	}
//	else
//		printf("\r\n");
}

static void ProgramStateReply(uCHAR Len, uCHAR *pDta)
{
//	_pMCURPYFMT pReplyDta = (_pMCURPYFMT)pDta;
	
//	printf("Into the %s fun!\r\n",__FUNCTION__);
//	
//	printf("pReplyDta->StartWord : 0x%04X\r\n",pReplyDta->StartWord );
//	printf("pReplyDta->Cmd : 0x%04X\r\n",pReplyDta->Cmd );
//	printf("pReplyDta->Result : 0x%04X\r\n",pReplyDta->Result );
//	printf("pReplyDta->DtaLen : 0x%04X\r\n",pReplyDta->DtaLen );
//	if(pReplyDta->DtaLen != 0)
//	{
//		printf("Dta :");
//		for(uINT Cnt = 0; Cnt < pReplyDta->DtaLen; Cnt++)
//			printf(" 0x%02X",*(pReplyDta->pDta + Cnt));
//		printf("\r\n\r\n");
//	}
//	else
//		printf("\r\n");
}

void (*HostFirmwareUpdateCmdFun[])(uCHAR, uCHAR *) = 
{
	UpdateParameterReply,							//0x00 : Update Parameter Reply
	EarseCodeReply,										//0x01 : Earse Code Reply
	WriteCodeReply,										//0x02 : Write Code Reply
	ReadCodeReply,										//0x03 : Read Code Reply
	StartUpdateReply,									//0x04 : Start Update Reply
	EndUpdateReply,										//0x05 : End Update Reply
	ReadOperationModeReply,						//0x06 : Read Operation mode Reply
	ReadApplicationNameReply,					//0x07 : Application Name Reply
	ProgramStateReply,								//0x08 : Program state Reply
};

pFun HostSoCCmdFun = 
{
#ifdef	APP_RUN
	CtrlReplyFun,
	WatchDogCtrlReplyFun,
	EventRespFun,
	MCUInfoReplyFun,
#else
	NULL,
	NULL,
	NULL,
	NULL,
#endif
	HostFirmwareUpdateCmdFun,
	NULL,
	NULL,
};


static void SoCCmdReq(uSHORT DtaLen, uCHAR *pDta)
{
	_pSOCCMD pCmdDta = (_pSOCCMD)(malloc(sizeof(_SOCCMD)));

	pCmdDta->Cmd = ((uSHORT)pDta[2]) | 
								 (((uSHORT)pDta[3]) << 8 & 0xff00);
	pCmdDta->DtaLen = ((uSHORT)pDta[6]) | 
										(((uSHORT)pDta[7]) << 8 & 0xff00);
	pCmdDta->pDta = (pDta+8);
	pCmdDta->CRC8 = pDta[8+pCmdDta->DtaLen];
	
	//Reply to Soc the system information.
	(*SoCCmdReplyFun[(pCmdDta->Cmd&0xff00)>>8][pCmdDta->Cmd&0x00ff])(pCmdDta->DtaLen, pCmdDta->pDta);
	free(pCmdDta);
}


uCHAR HostCmdWaitReply = 0;
void SendHostSocCmd(_SOCUARTCMD CmdType, uSHORT DtaLen, uCHAR *pDta)
{
	uCHAR *pHostCmdBuf = malloc(DtaLen + 9);
	_pMCUCMDFMT pHostCmd = (_pMCUCMDFMT)pHostCmdBuf;
	
	HostCmdWaitReply = 1;
	//Send to MCU the system information.
	FiexdFileParSet(CmdType, DtaLen, pHostCmdBuf);
	memcpy(pHostCmd->pDta, pDta, DtaLen);
//	printf("Send Host Soc command!!!\r\n");
	SocUartSendCmd(pHostCmdBuf);
	
	free(pHostCmdBuf);
}

void SocUartCmdProcess(int len)
{
	if (len) {
		uCHAR CRC8_Result = 0;
		CRC8_Result = Get_CRC8((SocCmdBuf+2),len-1, 0);
		if(CRC8_Result ==  SocCmdBuf[len+2-1])
		{
			if(HostCmdWaitReply)
			{
				/*Process the Host command reply.*/
				((HostSoCCmdFun[SocCmdBuf[5]])[SocCmdBuf[4]])(len,(SocCmdBuf+2));

				HostCmdWaitReply = 0;
			}
			else
			{
//              printf("Receive 0x%02X%02X command request from SoC!\r\n",SocCmdBuf[5],SocCmdBuf[4]);
				if((SoCCmdReplyFun[SocCmdBuf[5]]) != NULL)
					SoCCmdReq(len,(SocCmdBuf+2));
				else
					printf("Receive Not supports command from SOC!Correct Value : 0x%02X\r\n\r\n",CRC8_Result);
			}
		}
		else
			printf("SoC receive CRC8 value error! 0x%02X 0x%02X\r\n\r\n", CRC8_Result, SocCmdBuf[len+2-1]);
	} 
	else
		printf("SocCmdBuf is empty %s\r\n", __func__);
}
