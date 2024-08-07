/*
 * uart-loopback.c - userspace uart loopback test
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>

static struct termio oterm_attr;

static int baudflag_arr[] = {
	B921600, B460800, B230400, B115200, B57600, B38400,
	B19200,  B9600,   B4800,   B2400,   B1800,  B1200,
	B600,    B300,    B150,    B110,    B75,    B50
};
static int speed_arr[] = {
	921600,  460800,  230400,  115200,  57600,  38400,
	19200,   9600,    4800,    2400,    1800,   1200,
	600,     300,     150,     110,     75,     50
};

int speed_to_flag(int speed)
{
	int i;

	for (i = 0;  i < sizeof(speed_arr)/sizeof(int);  i++) {
		if (speed == speed_arr[i]) {
			return baudflag_arr[i];
		}
	}

	fprintf(stderr, "Unsupported baudrate, use 9600 instead!\n");
	return B9600;
}

int setup_port(int fd, int baud, int databits, int parity, int stopbits)
{
	struct termio term_attr;


	if (ioctl(fd, TCGETA, &term_attr) < 0) {
		return -1;
	}


	memcpy(&oterm_attr, &term_attr, sizeof(struct termio));

	term_attr.c_iflag &= ~(INLCR | IGNCR | ICRNL | ISTRIP);
	term_attr.c_oflag &= ~(OPOST | ONLCR | OCRNL);
	term_attr.c_lflag &= ~(ISIG | ECHO | ICANON | NOFLSH);
	term_attr.c_cflag &= ~CBAUD;
	term_attr.c_cflag |= CREAD | speed_to_flag(baud);


	term_attr.c_cflag &= ~(CSIZE);
	switch (databits) {
	case 5:
		term_attr.c_cflag |= CS5;
		break;

	case 6:
		term_attr.c_cflag |= CS6;
		break;

	case 7:
		term_attr.c_cflag |= CS7;
		break;

	case 8:
	default:
		term_attr.c_cflag |= CS8;
		break;
	}


	switch (parity) {
	case 1:  
		term_attr.c_cflag |= (PARENB | PARODD);
		break;

	case 2:  
		term_attr.c_cflag |= PARENB;
		term_attr.c_cflag &= ~(PARODD);
		break;

	case 0:  
	default:
		term_attr.c_cflag &= ~(PARENB);
		break;
	}



	switch (stopbits) {
	case 2:  
		term_attr.c_cflag |= CSTOPB;
		break;

	case 1:  
	default:
		term_attr.c_cflag &= ~CSTOPB;
		break;
	}

	term_attr.c_cc[VMIN] = 1;
	term_attr.c_cc[VTIME] = 0;

	if (ioctl(fd, TCSETAW, &term_attr) < 0) {
		return -1;
	}

	if (ioctl(fd, TCFLSH, 2) < 0) {
		return -1;
	}

	return 0;
}

void print_usage(char *program_name)
{
	fprintf(stderr,
			"*************************************\n"
			"  A Simple Serial Port Test Utility\n"
			"*************************************\n\n"
			"Usage:\n  %s <device> <baud> <loops> \n"
			"Example:\n  %s /dev/ttymxc1 115200 1\n\n",
			program_name, program_name
		   );
}

int main(int argc, char **argv)
{
	fd_set readfs;	  /* file descriptor set */
	struct timeval Timeout;
	int fd,ret;
	char *tty;
	char j[2];
	const int begin=32;		// ascii printable character
	const int end=255;		// ascii extended character
	char i;
	int cnt = 0;
	int loops;

	int baud = 115200;
	int databits = 8;
	int stopbits = 1;
	int parity = 0;

	if (argc < 3) {
		print_usage(argv[0]);
		return 1;
	}


	if (argv[3]) {
		loops = atoi(argv[3]);
		fprintf(stderr, "loops:%d \n", loops);
	} else {
		loops = -1;
		fprintf(stderr, "loops forever \n");
	}


	baud = atoi(argv[2]);
	if ((baud < 0) || (baud > 921600)) {
		fprintf(stderr, "Invalid baudrate!\n");
		return 1;
	}

	tty = argv[1];

//	printf("count=%d\n",count);

	printf("uart port used: %s\n",tty);

	if ((fd = open (tty, O_RDWR | O_NOCTTY)) < 0) {
		perror(tty);
		exit(1);
	}

	if (setup_port(fd, baud, databits, parity, stopbits)) {
		fprintf(stderr, "setup_port error %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	while (loops != cnt) {

		tcflush(fd, TCIFLUSH);

		for (i = begin; i < end; i++) {
			ret=write(fd,&i, 1);
			if (ret != 1) {
				printf("write error!\n");
				exit(1);
			}

			FD_SET(fd, &readfs);  /* set testing source */

			/* set timeout value within input loop */
			Timeout.tv_usec = 10 * 1000;  /* milliseconds, the interval between two bytes is 500us~800us by measure on scope*/
			Timeout.tv_sec  = 0;  /* seconds */
			ret = select(fd+1, &readfs, NULL, NULL, &Timeout);
			if (ret == 0) {
				printf("read timeout error!\n");
				exit(1);
			} else
				ret=read(fd, &j, 1);

			if (ret != 1) {
				printf("read error!\n");
				exit(1);
			}

			// compare write and read ascii
			if (i != j[0]) {
				printf("read data error: wrote 0x%x read 0x%x\n",i,j[0]);
				exit(1);
			}

			//printf("%c",j[0]);
		}

		cnt++;
		if (end == i) {
			printf("test success %d\n", cnt);
		} else {
			printf("test fail %d\n", cnt);
			exit(1);
		}

	}

	close(fd);

	return 0;
}