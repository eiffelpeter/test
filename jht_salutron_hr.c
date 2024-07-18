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

#define MAX_REPLY_SIZE 128

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

int uart_init(void)
{
	struct termios s_alicat;

	fd = open("/dev/ttyS7", O_RDWR | O_NONBLOCK ); //O_NOCTTY | O_NDELAY);

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

unsigned short Fletcher16(const char *data, int count )
{
	unsigned short sum1 = 0;
	unsigned short sum2 = 0;
	unsigned short ChkSumLoByte;
	unsigned short ChkSumHiByte;
	int index;
	for (index = 0; index < count; ++index) {
		sum1 = (sum1 + data[index]) % 255;
		sum2 = (sum2 + sum1) % 255;
	}
	sum1 = (sum2 << 8) | sum1;
	// Compute the checksum bytes based on the summed bytes.
	ChkSumLoByte = 255 - (((sum1 & 0xFF) + (sum1 >> 8)) % 0xFF);
	ChkSumHiByte = 255 - ((ChkSumLoByte + (sum1 & 0xFF)) % 0xFF);
	return(ChkSumLoByte | (ChkSumHiByte << 8));
}

void hrm8700_get_system_status() {
	char data[8];
	unsigned short checksum;

	// calculate checksum
	data[4] = 0x01;
	data[5] = 0x1C;
	data[6] = 0x06;
	data[7] = 0x01;
	checksum = Fletcher16(&data[4], 4);

	// send command
	data[0] = 0x0B;
	data[1] = sizeof(data);	 // length
	data[2] = checksum & 0xFF;
	data[3] = checksum >> 8;
	uart_puts(data, sizeof(data));
}

void hrm8700_get_software_version() {
	char data[8];
	unsigned short checksum;

	// calculate checksum
	data[4] = 0x01;
	data[5] = 0x1C;
	data[6] = 0x06;
	data[7] = 0x02;
	checksum = Fletcher16(&data[4], 4);

	// send command
	data[0] = 0x0B;
	data[1] = sizeof(data);	 // length
	data[2] = checksum & 0xFF;
	data[3] = checksum >> 8;
	uart_puts(data, sizeof(data));
}

void hrm8700_start_workout(int rssi_option) {
#if 1
	char data[9];
	unsigned short checksum;

	data[4] = 0x01;
	data[5] = 0x1C;
	data[6] = 0x04;
	data[7] = rssi_option;
	data[8] = 0xFF;	// reserve
	checksum = Fletcher16(&data[4], 5);

	// send command
	data[0] = 0x0B;
	data[1] = sizeof(data);	 // length
	data[2] = checksum & 0xFF;
	data[3] = checksum >> 8;
#else
	char data[8];
	unsigned short checksum;

	data[4] = 0x01;
	data[5] = 0x1C;
	data[6] = 0x04;
	data[7] = rssi_option;  
	checksum = Fletcher16(&data[4], 4);

	// send command
	data[0] = 0x0B;
	data[1] = sizeof(data);	 // length
	data[2] = checksum & 0xFF;
	data[3] = checksum >> 8;
#endif
	uart_puts(data, sizeof(data));
}

void hrm8700_stop_workout(void) {
	char data[8];
	unsigned short checksum;

	data[4] = 0x01;
	data[5] = 0x1C;
	data[6] = 0x04;
	data[7] = 0x04; 
	checksum = Fletcher16(&data[4], 4);

	// send command
	data[0] = 0x0B;
	data[1] = sizeof(data);	 // length
	data[2] = checksum & 0xFF;
	data[3] = checksum >> 8;
	uart_puts(data, sizeof(data));
}

bool hrm8700_process_workout_rx_data(const char *data, const int length)
{
	int len = 0, idx = 0;
	unsigned short checksum;
	bool ret = false;

	while (idx < length) {

		if (data[idx + 0] == 0xA ) {
			len = data[idx + 1];

			checksum = Fletcher16(&data[idx + 4], len-4);  // -4 : command - length - checksum low - checksum high

			if ( checksum == (data[idx + 2] | data[idx + 3] << 8) ) {
				if ((data[idx + 4] == 0x00) && (data[idx + 5] == 0x0c) && (data[idx + 6] == 0x07) ) {
					printf("workout %s\n", (data[idx + 7] == 1) ? "success" : "fail");
					ret = (data[idx + 7] == 1) ? true : false;
				} else {
					printf("print unprocess rx 1 at %s\n", __func__);
					pr_buf(&data[idx], length-idx);
				}
			} else {
				printf("checksum not match \n");
				printf("0x%02X", checksum);
				printf("\n");
				printf("0x%02X",(data[idx + 2] | data[idx + 3] << 8));
				printf("\n");

				printf("print unprocess rx 2 at %s\n", __func__);
				pr_buf(&data[idx], length-idx);
			}

			idx += len;

		} else {
			printf("print unprocess rx 3 at %s\n", __func__);
			pr_buf(&data[idx], length-idx);
			idx++;
			//break;
		}
	}

	return ret;
}

void hrm8700_enable_hr_with_rr_interval(bool rr_enable) {
	char data[8];
	unsigned short checksum;

	// calculate checksum
	data[4] = 0x01;
	data[5] = 0x1C;
	data[6] = 0x08;
	data[7] = rr_enable ? 0x03 : 0x04;
	checksum = Fletcher16(&data[4], 4);

	// send command
	data[0] = 0x0B;
	data[1] = sizeof(data);	 // length
	data[2] = checksum & 0xFF;
	data[3] = checksum >> 8;
	uart_puts(data, sizeof(data));
}



bool hrm8700_process_rx_data(const char *data, const int length)
{
	int len, idx = 0;
	unsigned short checksum;
	bool ret = false;
	unsigned long ms;
	struct timeval curr_time;
	static struct timeval prev_time;

	gettimeofday(&curr_time,NULL);
	ms = (1000 * (curr_time.tv_sec-prev_time.tv_sec)) + ((curr_time.tv_usec-prev_time.tv_usec) / 1000);	// diff


	while (idx < length) {

		if (data[idx + 0] == 0xA ) {
			len = data[idx + 1];

			checksum = Fletcher16(&data[idx + 4], len-4);  // -4 : command - length - checksum low - checksum high

			if ( checksum == (data[idx + 2] | data[idx + 3] << 8) ) {
				if ((data[idx + 4] == 0x00) && (data[idx + 5] == 0x0c) && (data[idx + 6] == 0x07) ) {
					printf("workout %s\n", (data[idx + 7] == 1) ? "success" : "fail");
					ret = (data[idx + 7] == 1) ? true : false;
				} else if ((data[idx + 4] == 0x00) && (data[idx + 5] == 0x0e) && (data[idx + 6] == 0x30) && (data[idx + 7] == 0x01)) {
					printf("Workout has started already\n");
					ret = true;
				} else if ((data[idx + 4] == 0x00) && (data[idx + 5] == 0x0c) && (data[idx + 6] == 0x09) && (data[idx + 7] == 0x03)) {
					printf("[%ld] hand on\n", ms);
					ret = true;
				} else if ((data[idx + 4] == 0x00) && (data[idx + 5] == 0x0c) && (data[idx + 6] == 0x09) && (data[idx + 7] == 0x04)) {
					printf("[%ld] hand off\n", ms);
					ret = true;
				} else if ((data[idx + 4] == 0x00) && (data[idx + 5] == 0x0c) && (data[idx + 6] == 0x23) ) {
					printf("[%ld] hr: ", ms);
					printf("%d", data[idx + 7]);
					printf("\n");
					ret = true;
				} else if ((data[idx + 4] == 0x00) && (data[idx + 5] == 0x0F) && (data[idx + 6] == 0x14) ) {
					printf("[%ld] rr: ", ms);
					printf("%d", ((data[idx + 8] << 8) | data[idx + 7]));
					printf("\n");
					ret = true;
				} else {
					printf("print unprocess rx 1 at %s\n", __func__);
					pr_buf(&data[idx], length-idx);
				}
			} else {
				printf("checksum not match \n");
				printf("0x%02X", checksum);
				printf("\n");
				printf("0x%02X",(data[idx + 2] | data[idx + 3] << 8));
				printf("\n");

				printf("print unprocess rx 2 at %s\n", __func__);
				pr_buf(&data[idx], length-idx);
			}

			idx += len;

		} else {
			printf("print unprocess rx 3 at %s\n", __func__);
			pr_buf(&data[idx], length-idx);
			break;
		}
	}

	prev_time = curr_time;

	return ret;
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
	//hrm8700_get_software_version();

	hrm8700_start_workout(1);
	len = uart_gets(rx_buf, MAX_REPLY_SIZE);
	if (len > 0)
		hrm8700_process_workout_rx_data(rx_buf, len);

	usleep(500*1000); // wait at least 500ms

	hrm8700_enable_hr_with_rr_interval(true); // true: HR+RR,  false:HR	

	while (run_flag) {
		len = uart_gets(rx_buf, MAX_REPLY_SIZE);
		if (len > 0) {
			// process rx
			hrm8700_process_rx_data(rx_buf, len);
		}
	}

	hrm8700_stop_workout();

	close(fd);
	return ret;
}
