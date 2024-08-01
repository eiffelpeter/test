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
#include <asm-generic/ioctls.h> /* TIOCGRS485 + TIOCSRS485 ioctl definitions */
#include <linux/serial.h>

#define MAX_REPLY_SIZE 128

typedef enum {
	BIKE = 0,
	TREADMILL = 1,
} JHT_LCB_TYPE_t;

static int fd;
static int debug = 0;
static JHT_LCB_TYPE_t lcb_type = BIKE;
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
		return -1;
	}

	if (level == 0)
		status |= TIOCM_RTS;	// RTS low
	else
		status &= ~TIOCM_RTS;	// RTS high

	if (ioctl(fd, TIOCMSET, &status) == -1) {
		perror("setRTS(): TIOCMSET");
		return -1;
	}
	return 0;
}

int rs485_enable(const int fd, bool enable)
{
	struct serial_rs485 rs485conf;
	int res;

	/* Get configure from device */
	res = ioctl(fd, TIOCGRS485, &rs485conf);
	if (res < 0) {
		perror("Ioctl error on getting 485 configure:");
		return res;
	}

	/* Set enable/disable to configure */
	if (enable) {	// Enable rs485 mode
		rs485conf.flags |= SER_RS485_ENABLED;
	} else {		// Disable rs485 mode
		rs485conf.flags &= ~(SER_RS485_ENABLED);
	}

	/* Set configure to device */
	res = ioctl(fd, TIOCSRS485, &rs485conf);
	if (res < 0) {
		perror("Ioctl error on setting 485 configure:");
	}

	return res;
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
	} else {
		printf("open fail \n");
		return -1;
	}
}

int uart_puts(const char *msg, int len)
{
	int n = 0;

	n = write(fd, msg, len);

	if (debug) {
		printf("Uart_write - len:%d\n", n);
		printBytes(msg, n);
	}

	return n;
}

int uart_gets(char *msg, int len)
{
	fd_set fds;
	struct timeval timeout;
	int ret;
	int n = 0;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	timeout.tv_sec = 0;
	timeout.tv_usec = 100 * 1000;	// 100ms
	ret = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
	if (ret == 1) {
		n = read(fd, msg, len);
	}

	if (debug) {
		if (n > 0) {
			printf("Uart_read - len:%d \n", n); 
			printBytes(msg, n);
		}
	}

	return n;
}

void jht_get_version(void) {
	char data[ ] = {0x00, 0xFF, 0x73, 0x00, 0x00};
	int cmd_size = sizeof(data);

	data[cmd_size-1] = Get_CRC8(data, cmd_size-1);
	uart_puts(data, cmd_size);
}

void jht_get_status(void) {
	char data[ ] = {0x00, 0xFF, 0x71, 0x00, 0x00};
	int cmd_size = sizeof(data);

	data[cmd_size-1] = Get_CRC8(data, cmd_size-1);
	uart_puts(data, cmd_size);
}

void jht_get_error_code(void) {
	char data[ ] = {0x00, 0xFF, 0x72, 0x00, 0x00};
	int cmd_size = sizeof(data);

	data[cmd_size-1] = Get_CRC8(data, cmd_size-1);
	uart_puts(data, cmd_size);
}

void jht_get_rpm(void) {
	char data[ ] = {0x00, 0xFF, 0x63, 0x00, 0x00};	// bike
	int cmd_size = sizeof(data);

	if (lcb_type == TREADMILL) {
		data[2] = 0xF9;		// treadmill
	}

	data[cmd_size-1] = Get_CRC8(data, cmd_size-1);
	uart_puts(data, cmd_size);
}

int main(int argc, char **argv) {

	int ret = 0;
	char rx_buf[MAX_REPLY_SIZE];
	int len;

	// Register signals
	signal(SIGINT, exit_handler);

	if (argv[1] != NULL) {
		if (0 == strcmp(argv[1], "--debug")) {
			debug = 1;
			printf("print uart trx \n");
		} else {
			printf("unsupport command:%s\n", argv[1]);
			return -1;
		}
	}

	if ( 0 != uart_init()) {
		return -1;
	}

	if ( rs485_enable(fd, true) < 0 )
		printf("rs485_enable fail\n");

	// set fixture to URE 30

	jht_get_status();
	len = uart_gets(rx_buf, MAX_REPLY_SIZE);
	sleep(1);

	if (rx_buf[3] != 0) {
		jht_get_error_code();
		len = uart_gets(rx_buf, MAX_REPLY_SIZE);
		sleep(1);

		jht_get_status();
		len = uart_gets(rx_buf, MAX_REPLY_SIZE);
		sleep(1);
	}

	jht_get_version();
	len = uart_gets(rx_buf, MAX_REPLY_SIZE);
	printf("version: %x %x\n", rx_buf[5], rx_buf[4]);

	if ((rx_buf[5] == 4) || (rx_buf[5] == 7)) {
		lcb_type = BIKE;
	} else {
		lcb_type = TREADMILL;
	}


	while (run_flag) {
		jht_get_rpm();

		len = uart_gets(rx_buf, MAX_REPLY_SIZE);
		if (len > 0) {
			// process rx

			// is header 0x01 ?
			if (rx_buf[0] == 0x01) {
				// checkk CRC
				if (Get_CRC8(rx_buf, len-1) == rx_buf[len-1]) {
					// print rpm
					if (rx_buf[2] == 0x63)
						printf("rpm: %d\n", (rx_buf[4] << 8) | rx_buf[5]);
					else if (rx_buf[2] == 0xF9)
						printf("rpm: %d\n", (rx_buf[4] << 8) | rx_buf[5]);
				} else {
					printf("crc8 is dismatch 0x%02X 0x%02X\n", Get_CRC8(rx_buf, len-1), rx_buf[len-1]);
				}
			} else {
				printf("header is not 0x01\n");
			}
		}
		sleep(1);
	}

	close(fd);
	return ret;
}
