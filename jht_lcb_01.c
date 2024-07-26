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
#include <sys/ioctl.h>


#define MAX_REPLY_SIZE 128

#define DEBUG 1

int fd;
volatile sig_atomic_t run_flag = 1;

static void exit_handler(int sig) {	// can be called asynchronously
	run_flag = 0;					// set flag
}

void printBytes(const char *data, int len)
{
	int i;

	for (i = 0; i < len; i++)
		printf("0x%02X ", data[i]);
	printf("\n");
}

static unsigned char Lcb_CRC8_Table[16] =
{ 0x00, 0x31, 0x62, 0x53, 0xC4, 0xF5, 0xA6, 0x97, 0xB9, 0x88,
	0xDB, 0xEA, 0x7D, 0x4C, 0x1F, 0x2E};

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
//*****************************************************************************
//* Function Name  : DEV_RS485_Get_CRC8
//* Description    :
//* Input          : None
//* Output         : None
//* Return         : None
//*****************************************************************************
unsigned char Get_CRC8(char *Ptr, unsigned char DtaLen)
{
	unsigned char  CRC_Count;
	unsigned char  CRC_Temp, CRC_Half, CRC_Data = 0;
	unsigned char  *pCrcTable; 

	pCrcTable = Lcb_CRC8_Table;

	for (CRC_Count = 0; CRC_Count < DtaLen; CRC_Count++) {
		CRC_Temp   = *(Ptr + CRC_Count);
		CRC_Half   = (CRC_Data / 16);
		CRC_Data <<= 4;
//  CRC_Data  ^= CRC8_Table[ CRC_Half ^ (CRC_Temp / 16)];
		CRC_Data  ^= pCrcTable[ CRC_Half ^ (CRC_Temp / 16)];
		CRC_Half   = (CRC_Data / 16);
		CRC_Data <<= 4;
//  CRC_Data  ^= CRC8_Table[ CRC_Half ^ (CRC_Temp & 0x0F)];
		CRC_Data  ^= pCrcTable[ CRC_Half ^ (CRC_Temp & 0x0F)];
	}
	return  CRC_Data;
}

int setRTS(int fd, int level)
{
	int status;

	if (ioctl(fd, TIOCMGET, &status) == -1) {
		perror("setRTS(): TIOCMGET");
		return 0;
	}
	if (level == 0)
		status |= TIOCM_RTS;	// RTS low
	else
		status &= ~TIOCM_RTS;	// RTS high
	if (ioctl(fd, TIOCMSET, &status) == -1) {
		perror("setRTS(): TIOCMSET");
		return 0;
	}
	return 1;
}

int uart_init(void)
{
	struct termios s_alicat;

	fd = open("/dev/ttyS5", O_RDWR | O_NONBLOCK ); //O_NOCTTY | O_NDELAY);

	if (fd != -1) {

		if (tcgetattr(fd, &s_alicat) < 0) {
			printf("Error from tcgetattr: %s\n", strerror(errno));
			return -1;
		}

		cfsetospeed(&s_alicat, B9600);
		cfsetispeed(&s_alicat, B9600);

		// https://www.cs.uleth.ca/~holzmann/C/system/ttyraw.c
		/* input modes - clear indicated ones giving: no break, no CR to NL, 
		   no parity check, no strip char, no start/stop output (sic) control */
		s_alicat.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

		/* output modes - clear giving: no post processing such as NL to CR+NL */
		s_alicat.c_oflag &= ~(OPOST);

		/* control modes - set 8 bit chars */
		s_alicat.c_cflag |= (CS8);//| CRTSCTS);

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
	} else {
		printf("open fail \n");
		return -1;
	}
}

int uart_puts(const char *msg, int len)
{
	int n = 0;

	// for JHT LCB
	usleep(10*1000);
	setRTS(fd, 1);  

	n = write(fd, msg, len);

#ifdef DEBUG
	printf("Uart_write - len:%d\n", n);
	printBytes(msg, n);
#endif
	return n;
}

int uart_gets(char *msg, int len)
{
	fd_set fds;
	struct timeval timeout;
	int ret;
	int n = 0;

	// for JHT LCB
	usleep(10*1000);
	setRTS(fd, 0);


	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	timeout.tv_sec = 0;
	timeout.tv_usec = 200 * 1000;	// 200ms
	ret = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
	if (ret == 1) {
		n = read(fd, msg, len);
	}

#ifdef DEBUG
	if (n > 0) {
		printf("Uart_read - len:%d \n", n); 
		printBytes(msg, n);
	}
#endif
	return n;
}



void pr_buf(const char * data, int length)
{
	int i;
	for (i=0; i<length; i++) {
		printf("0x%02X", data[i]);
		printf(" ");
	}
	printf("\n");
}




void jht_get_version(void) {
	char data[5];   

	data[0] = 0x00;
	data[1] = 0xFF;
	data[2] = 0x73;
	data[3] = 0x00;
	data[4] = Get_CRC8(data, 4);
	uart_puts(data, 5);
}

void jht_get_status(void) {
	char data[5];   

	data[0] = 0x00;
	data[1] = 0xFF;
	data[2] = 0x71;
	data[3] = 0x00;
	data[4] = Get_CRC8(data, 4);
	uart_puts(data, 5);
}

void jht_get_error_code(void) {
	char data[5];   

	data[0] = 0x00;
	data[1] = 0xFF;
	data[2] = 0x72;
	data[3] = 0x00;
	data[4] = Get_CRC8(data, 4);
	uart_puts(data, 5);
}

void jht_lcb_initialize(void) {
	char data[5];   

	data[0] = 0x00;
	data[1] = 0xFF;
	data[2] = 0x70;
	data[3] = 0x00;
	data[4] = Get_CRC8(data, 4);
	uart_puts(data, 5);
}

void jht_get_rpm(void) {
	char data[5];   

	data[0] = 0x00;
	data[1] = 0xFF;
	data[2] = 0xF9;
	data[3] = 0x00;
	data[4] = Get_CRC8(data, 4);
	uart_puts(data, 5);
}

void jht_set_operate_status(void) {
	char data[6];   

	data[0] = 0x00;
	data[1] = 0xFF;
	data[2] = 0xF5;
	data[3] = 0x01;
	data[4] = 0x00;
	data[5] = Get_CRC8(data, sizeof(data)-1);
	uart_puts(data, sizeof(data));
}

int main(int argc, char **argv) {

	int ret = 0;
	char rx_buf[MAX_REPLY_SIZE];
	int len;

	// Register signals
	signal(SIGINT, exit_handler);

	if ( 0 != uart_init()) {
		return -1;
	}
	setRTS(fd, 0); // set to listen mode 
	usleep(10*1000);
	

	jht_lcb_initialize();
	len = uart_gets(rx_buf, MAX_REPLY_SIZE);
	sleep(1);

	jht_get_error_code();
	len = uart_gets(rx_buf, MAX_REPLY_SIZE);
	sleep(1);

	jht_set_operate_status();
	len = uart_gets(rx_buf, MAX_REPLY_SIZE);
	sleep(1);

	while (run_flag) {
		jht_get_rpm();

		len = uart_gets(rx_buf, MAX_REPLY_SIZE);
		if (len > 0) {
			// process rx
			// hrm8700_process_rx_data(rx_buf, len);
		}

        sleep(1);
	}

	close(fd);
	return ret;
}
