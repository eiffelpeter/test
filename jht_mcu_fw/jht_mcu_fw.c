/*
 * Boot support
 */

#include <fcntl.h>
#include <signal.h> // signal functions
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>

#include "jht_mcu_fw.h"
#include "main.h"
#include "SocUart.h"
#include "MemoryFlashWriteRead.h"

#define UART_PORT "/dev/ttymxc3" // "/dev/ttyS4" // "/dev/ttymxc1" // 
//#define DEBUG 1

unsigned char FwDta[0x300] = {0};

uCHAR ReadBinTemp[0x230];
extern _pMEMINF pMemInf;
extern uCHAR McuCurrentMode;


const unsigned char CRC8_Table[16] = {0x00, 0x5A, 0xB4, 0xEE, 0x32, 0x68, 0x86, 0xDC, 0x64, 0x3E, 0xD0, 0x8A, 0x56, 0x0C, 0xE2, 0xB8};

unsigned char get_CRC8(char *ptr, unsigned int length)
{
	unsigned int CRC_COUNT;
	char HIGH_NIBBLE, DATA_TEMP, NEW_CRC = 0;

	CRC_COUNT = length;
	while (CRC_COUNT-- != 0) {
		DATA_TEMP = *ptr;
		HIGH_NIBBLE = NEW_CRC / 16;
		NEW_CRC <<= 4;
		NEW_CRC ^= CRC8_Table[HIGH_NIBBLE ^ (DATA_TEMP / 16)];
		HIGH_NIBBLE = NEW_CRC / 16;
		NEW_CRC <<= 4;
		NEW_CRC ^= CRC8_Table[HIGH_NIBBLE ^ (DATA_TEMP & 0x0F)];
		ptr++;
	}
	return NEW_CRC;
}

void printBytes(const char *data, int len)
{
	int i;

	if (len) {
		for (i = 0; i < len; i++) {
			printf("0x%02X ", data[i]);

			if((i%16) == 15)
				printf("\n");
		}	
		printf("\n");
	}
}

static int fd;
int chimera_uart_init(void)
{
	struct termios s_alicat;
	fd = open(UART_PORT, O_RDWR | O_NOCTTY | O_NDELAY);

	if (fd != -1) {
		if (tcgetattr(fd, &s_alicat) < 0) {
			printf("Error from tcgetattr: %s\n", strerror(errno));
			return -1;
		}
	
		cfsetospeed(&s_alicat, B115200);
		cfsetispeed(&s_alicat, B115200);


		// https://www.cs.uleth.ca/~holzmann/C/system/ttyraw.c
		/* input modes - clear indicated ones giving: no break, no CR to NL, 
		   no parity check, no strip char, no start/stop output (sic) control */
		s_alicat.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	
		/* output modes - clear giving: no post processing such as NL to CR+NL */
		s_alicat.c_oflag &= ~(OPOST);
	
		/* control modes - set 8 bit chars */
		s_alicat.c_cflag |= (CS8);
	
		/* local modes - clear giving: echoing off, canonical off (no erase with 
		   backspace, ^U,...),  no extended functions, no signal chars (^Z,^C) */
		s_alicat.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);


    /* control chars - set return condition: min number of bytes and timer */
//  s_alicat.c_cc[VMIN] = 5; s_alicat.c_cc[VTIME] = 8; /* after 5 bytes or .8 seconds
//                                              after first byte seen      */
//  s_alicat.c_cc[VMIN] = 0; s_alicat.c_cc[VTIME] = 0; /* immediate - anything       */
//  s_alicat.c_cc[VMIN] = 2; s_alicat.c_cc[VTIME] = 0; /* after two bytes, no timer  */
//  s_alicat.c_cc[VMIN] = 0; s_alicat.c_cc[VTIME] = 8; /* after a byte or .8 seconds */


		// https://gist.github.com/Hazanel/2c0ea98184278b4af81d17a97ab1c636#file-uart_driver-c-L125L-L128

		tcflush(fd, TCIFLUSH); //discard file information not transmitted
		if (tcsetattr(fd, TCSANOW, &s_alicat) != 0) {
			printf("Error from tcsetattr: %s\n", strerror(errno));
			return -1;
		}
		return 0;
	}
	else {
		printf("open uart fail \n");
		return -1;
	}
}

int chimera_uart_deinit(void)
{
	if (fd)
		close(fd);

	return 0;
}

int chimera_uart_puts(const char *msg, int len)
{
	int n = 0;
#if DEBUG
	printf("Uart_write - len:%d\n", len);
	printBytes(msg, len);	
#endif
	n = write(fd, msg, len);
	return n;
}

int chimera_uart_gets(char *msg, int len)
{
	fd_set fds;
	struct timeval timeout;
	int ret;
	int n = 0;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	timeout.tv_sec = 0;
	timeout.tv_usec = 100 * 1000;	// 100ms ,   0x230 * (8 data + 1 start bit + 1 stop bit) / 115200 = 48.61ms
	ret = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
	if (ret == 1) {
		n = read(fd, msg, len);         
	}
#if DEBUG
	{
		printf("Uart_read - len:%d \n", n); 
		printBytes(msg, n);
	}
#endif
    return n;
}

int parseReplyPackage(char *replyPackage, int replyLen, char *replyPayload)
{
	char replyCrc;
	char *replayPackagePtr = replyPackage;
	int replyPayloadLen;

	if (replyLen >= 9) {
		replyCrc = get_CRC8(replyPackage, replyLen - 1);
		if (replyCrc != replyPackage[replyLen - 1]) {
			printf("CRC check failed!\n");
			return -1;
		}
		if (replyPackage[0] == 0x55 && replyPackage[1] == 0xAA) {
			unsigned int val = (replyPackage[2] | replyPackage[3] << 8);

			switch (val) {
			case CMD_SYSTEM_STATE:
			case CMD_RECOVERY_STATE:
			case CMD_VERSION:
				break;
			default:
				printf("No supported command! cmd =%d\n", val);
				return -1;
			}
			val = (replyPackage[4] | replyPackage[5] << 8);
			switch (val) {
			case RST_SUCCESS:
				break;
			case RST_UNKNOWN_COMMAND:
			case RST_INVALID_PAYLOAD_LENGTH:
			case RST_INVALID_PARAMETERS:
			case RST_INVALID_ADDRESS:
			case RST_INVALID_PASSWORD:
			case RST_INVALID_FAILURE:
				printf("Fail or unknown result! ret=%d\n", val);
				return -1;
			}
			val = replyPackage[6] | (replyPackage[7] << 8);
			if (val > 0) {
				replyPayloadLen = replyLen - 9;
				if (replyPayloadLen == val) {
					replayPackagePtr += 8;
					memcpy(replyPayload, replayPackagePtr, replyPayloadLen);
				} else {
					printf("Payload data expected:%d actual:%d!", val, replyPayloadLen);
					return -1;
				}
			} else {
				replyPayloadLen = 0;
			}
			return replyPayloadLen;
		}
	}
	return -1;
}

int writeCommand(int cmd, char *readBuff, char *payload, int payloadLen)
{
	char packageCmd[MAX_BUFF_SIZE] = {0};
	char *packagePtr = packageCmd;
	int cmdLen = 0;
	int ret;

	packageCmd[0] = 0x55;
	packageCmd[1] = 0xAA;
	packageCmd[2] = (char)(cmd & 0xFF);
	packageCmd[3] = (char)((cmd >> 8) & 0xFF);
	/* Keep 0 in reserved blank. */
	packageCmd[4] = 0;
	packageCmd[5] = 0;
	packageCmd[6] = (char)(payloadLen & 0xFF);
	packageCmd[7] = (char)((payloadLen >> 8) & 0xFF);
	cmdLen += 8;
	packagePtr += cmdLen;
	memcpy(packagePtr, payload, payloadLen);
	cmdLen += payloadLen;
	packagePtr += payloadLen;
	*packagePtr = get_CRC8(packageCmd, cmdLen);
	cmdLen++;

	chimera_uart_puts(packageCmd, cmdLen);
	ret = chimera_uart_gets(readBuff, MAX_REPLY_SIZE);

	return ret;
}

int writeCommandNoPayload(int cmd, char *readBuff)
{
	char packageCmd[MAX_BUFF_SIZE] = {0};
	char *packagePtr = packageCmd;
	int cmdLen = 0;
	int ret;

	packageCmd[0] = 0x55;
	packageCmd[1] = 0xAA;
	packageCmd[2] = (char)(cmd & 0xFF);
	packageCmd[3] = (char)((cmd >> 8) & 0xFF);
	/* Keep 0 in reserved blank. */
	packageCmd[4] = 0;
	packageCmd[5] = 0;
	packageCmd[6] = 0;
	packageCmd[7] = 0;
	cmdLen += 8;
	packagePtr += cmdLen;
	*packagePtr = get_CRC8(packageCmd, cmdLen);
	cmdLen++;

	chimera_uart_puts(packageCmd, cmdLen);
	ret = chimera_uart_gets(readBuff, MAX_REPLY_SIZE);

	return ret;
}

int getSystemState(char *replyPayload, char classVal)
{
	char payload[1] = {classVal};
	char replyPackage[MAX_REPLY_SIZE] = {0};
	int ret;

	ret = writeCommand(CMD_SYSTEM_STATE, replyPackage, payload, sizeof(payload));
	if (ret > 0)
		ret = parseReplyPackage(replyPackage, ret, replyPayload);
	else
		ret = 0;

	return ret;
}

int getRecoveryState(char *replyPayload)
{
	char replyPackage[MAX_REPLY_SIZE] = {0};
	int ret;

	ret = writeCommandNoPayload(CMD_RECOVERY_STATE, replyPackage);
	if (ret > 0)
		ret = parseReplyPackage(replyPackage, ret, replyPayload);
	else
		ret = 0;

    return ret;
}

int getVersion(char *replyPayload)
{
	char replyPackage[MAX_REPLY_SIZE] = {0};
	int ret;

	ret = writeCommandNoPayload(CMD_VERSION, replyPackage);
	if (ret > 0)
		ret = parseReplyPackage(replyPackage, ret, replyPayload);
	else
		ret = 0;

    return ret;
}

int main(int argc, char **argv) {

    int ret = 0;
    char version[2];

	if (argv[1] == NULL) {
		printf("please enter bin file\n");
		return -1;
	}

	printf("open bin file %s\n", argv[1]);

	// open bin file 
    FILE *fp = fopen(argv[1], "rb+");	// adb push MCU_0002.bin /data/

	if (fp) {

		// init tty
		if ( 0 != chimera_uart_init() )
		{
			fclose(fp);
			return -1;
		}

		if ( getVersion(version) > 0 )
		{
			printf("version %d %d\n", version[0], version[1]);
			printf("\n");
		}

		printf("Start_Update\n");
		memcpy(FwDta, "johnson", 7);
		SendHostSocCmd(Start_Update, 7, FwDta);			//Start Update flowchar to Unlock the Iap mode.
		SoCUartReceiveCtrl();
		while(HostCmdWaitReply);
		memset(FwDta, 0, 7);
		sleep(3);

//  	printf("Application_Name\n");
//  	SendHostSocCmd(Application_Name, 0, NULL);      //Read the machine name.
//  	SoCUartReceiveCtrl();
		// while(HostCmdWaitReply);						//Waiting the command reply...

		printf("Update_Parameter\n");
		SendHostSocCmd(Update_Parameter, 0, NULL);		//Read the MCU memory status.
		SoCUartReceiveCtrl();
		// while(HostCmdWaitReply);						//Waiting the command reply...

		SendHostSocCmd(Earse_Code, 0, NULL);			//Erase the MCU memory flash.
		SoCUartReceiveCtrl();
		sleep(3);										//Waiting the MCU erase command success.

		uLONG offset = 0;
		uLONG write_len;
		int idx = 0;
	

		fseek(fp, 0, SEEK_END);
		uLONG bin_size = ftell(fp);
		printf("bin size :%ld\r\n", bin_size);

		pMemInf->AppEndAddr = bin_size + APP_FLASH_PARTITION;
		pMemInf->AppStartAddr = APP_FLASH_PARTITION;
		while(pMemInf->AppStartAddr < pMemInf->AppEndAddr)
		{
			uCHAR RWCtl[0x230];
			_pREADWRTIECTL pRWCtl = (_pREADWRTIECTL)RWCtl;
	
			pRWCtl->StartAddr = pMemInf->AppStartAddr;
			write_len = (pMemInf->AppEndAddr - pMemInf->AppStartAddr);
			write_len = (write_len < 0x200) ? write_len : 0x200;
			pRWCtl->Length = write_len;

			

			//  memcpy(pRWCtl->pDta, (uCHAR *)(pRWCtl->StartAddr), pRWCtl->Length);
            fseek(fp, offset, SEEK_SET);
            fread(pRWCtl->pDta, pRWCtl->Length, 1, fp);

			SendHostSocCmd(Write_Code, 520, RWCtl);			//Write the MCU memory flash.
			SoCUartReceiveCtrl();
			while(HostCmdWaitReply);						//Waiting the command reply...

			SendHostSocCmd(Read_Code, 8, RWCtl);			//Read the MCU memory flash.
			SoCUartReceiveCtrl();
			while(HostCmdWaitReply);						//Waiting the command reply...
			if(memcmp(pRWCtl->pDta, ReadBinTemp+4, pRWCtl->Length) == 0)
			{
				printf("Update:%d %%, idx:%d, write_len:%ld, offset:%ld\r\n",(uINT)((offset*100)/(bin_size)), idx, write_len, offset);

				pMemInf->AppStartAddr += write_len;
				offset += write_len;						// for next fseek
				idx++;
//		        printf("Write/Read flowchar is success!!!\r");
			}
			else
			{
				printf("Write/Read flowchar is fail!!!\r\n");
				printf("Send Dta :");
				for(uINT Cnt=0; Cnt<pRWCtl->Length; Cnt++)
				{
					if((Cnt%16) == 0)
						printf("\r\n0x%02X ", *(pRWCtl->pDta+Cnt));
					else
						printf("0x%02X ", *(pRWCtl->pDta+Cnt));
				}
				printf("\r\n\r\n");
	
				printf("Receive Dta :");
				for(uINT Cnt=0; Cnt<pRWCtl->Length; Cnt++)
				{
					if((Cnt%16) == 0)
						printf("\r\n0x%02X ", *(ReadBinTemp+4+Cnt));
					else
						printf("0x%02X ", *(ReadBinTemp+4+Cnt));
				}
				printf("\r\n\r\n");
				while(1);
			}
		}

		fclose(fp);

		if (pMemInf->AppStartAddr == pMemInf->AppEndAddr) {
			printf("Update:100 %%\r\n");
		}

		SendHostSocCmd(End_Update, 0, NULL);					//End the MCU update mode.
		sleep(3);
		SoCUartReceiveCtrl();
		//while(HostCmdWaitReply);											//Waiting the MCU erase command success.

WaitTheMcuJumpApp:
		sleep(3);
		printf("Operation_Mode \n\r");
		SendHostSocCmd(Operation_Mode, 0, NULL);			//Read the MCU Current Operation Mode.
		SoCUartReceiveCtrl();
		while(HostCmdWaitReply);                                            //Waiting the MCU erase command success.
		if(McuCurrentMode == 1)
			goto WaitTheMcuJumpApp;
	
		sleep(1);
		if ( getVersion(version) > 0 )
		{
			printf("version %d %d\n", version[0], version[1]);
			printf("\n");
		}

		chimera_uart_deinit();

	} else {
		printf("open bin fail \n");
		return -1;
	}

	return ret;
}
