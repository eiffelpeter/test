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

#include "jht_mcu.h"

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

	for (i = 0; i < len; i++)
		printf("0x%02X ", data[i]);
}

void printBytesDebug(char *msg, char *data, int len)
{
	printf("%s\n", msg);
	printBytes(data, len);
	printf("\n");
}

int leo_uart4_puts(const char *msg, int len)
{
	int n = 0;
	struct termios s_alicat;

	int fd = open("/dev/ttymxc3", O_RDWR | O_NOCTTY | O_NDELAY);

	if (fd != -1) {
		if (tcgetattr(fd, &s_alicat) < 0) {
			printf("Error from tcgetattr: %s\n", strerror(errno));
			return -1;
		}
	
		cfsetospeed(&s_alicat, B115200);
		cfsetispeed(&s_alicat, B115200);
	
		tcflush(fd, TCIFLUSH); //discard file information not transmitted
		if (tcsetattr(fd, TCSANOW, &s_alicat) != 0) {
			printf("Error from tcsetattr: %s\n", strerror(errno));
			return -1;
		}

		n = write(fd, msg, len);
		close(fd);

		printf("Uart_write - len:%d\n", n);
		printBytes(msg, n);
		printf("\n");

		return n;
	} else {
		printf("open fail \n");
		return -1;
	}
}

int leo_uart4_gets(char *msg, int len)
{
	fd_set fds;
	struct timeval timeout;
	int ret;
	int n = 0;
	struct termios s_alicat;

	int fd = open("/dev/ttymxc3", O_RDWR | O_NONBLOCK ); //O_NOCTTY | O_NDELAY);

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
	
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		timeout.tv_sec = 0;
		timeout.tv_usec = 1000 * 1000;	// 1000ms
		ret = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
		if (ret == 1) {
			n = read(fd, msg, len);         
		}
		close(fd);

        printf("Uart_read - len:%d \n", n); 
        printBytes(msg, n);
        printf("\n");

        return n;
	} else {
		printf("open fail \n");
		return -1;
	}
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

	leo_uart4_puts(packageCmd, cmdLen);
	ret = leo_uart4_gets(readBuff, MAX_REPLY_SIZE);

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

	leo_uart4_puts(packageCmd, cmdLen);
	ret = leo_uart4_gets(readBuff, MAX_REPLY_SIZE);

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
	ret = getVersion(version);
	printf("version %d %d\n", version[0], version[1]);
	printf("\n");

	char recover_state[1];
	ret = getRecoveryState(recover_state);
	printf("recover_state %d \n", recover_state[0]);
	printf("\n");

	return ret;
}
