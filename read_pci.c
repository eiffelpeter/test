#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
int main(int argc, char **argv) {
    char data[14];
    const int size = 6;         // 0x1234
    const int timeout_sec = 1;  // 1sec
    int fd;
    fd_set fds;
    struct timeval timeout;
    int count = 0;
    int ret;
    int n, i;

    const char * pcie_node[] = { 
        "/sys/bus/pci/devices/0001:01:00.0/vendor",
        "/sys/bus/pci/devices/0001:01:00.0/device",
        NULL
    };

    // read vid and pid of pcie
    count = 0;
    memset(data, 0, sizeof(data));
    i=0;


    while(pcie_node[i] != NULL) {
        printf("open %s \n", pcie_node[i]); 
        fd = open(pcie_node[i], O_RDONLY);
        i++;

        if(fd) {
            //do {
            FD_ZERO(&fds);
            FD_SET(fd, &fds);
            timeout.tv_sec = timeout_sec;
            timeout.tv_usec = 0;
            ret = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
            if(ret == 1) {
                n = read(fd, &data[count], size);

                printf("had read size:%d \n", n);

                count += n;
            } else
                printf("read timeout:%d sec \n", timeout_sec);

            //} while (ret == 1);

            close(fd);
            printf("total read size:%d \n", count);
        } else {
            printf("open fail \n");
            return -1;
        }
    }

    // print the raw data 
    printf("data:");
    for(n = 0; n < count; n++) {
        printf("%c", data[n]);
    }
    printf("\n");


    if ((data[2]=='1') && (data[3]=='0') && (data[4]=='e') && (data[5]=='c') &&
              (data[8]=='b') && (data[9]=='8') && (data[10]=='5') && (data[11]=='2')) {
            printf("found pcie rtl8852be \n");  // 0x10ec 0xb852
    } else if ((data[2]=='1') && (data[3]=='b') && (data[4]=='4') && (data[5]=='b') &&
           (data[8]=='2') && (data[9]=='b') && (data[10]=='4') && (data[11]=='2')) {
            printf("found pcie8997 \n");        // 0x1b4b 0x2b42
    } else {
        printf("not found  \n");
    }

    return count;
}
