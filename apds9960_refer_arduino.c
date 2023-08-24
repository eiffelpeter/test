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

#define DEBUG 0

typedef unsigned char uint8_t;

enum {
    GESTURE_NONE = -1,
    GESTURE_UP = 0,
    GESTURE_DOWN = 1,
    GESTURE_LEFT = 2,
    GESTURE_RIGHT = 3
};

volatile sig_atomic_t run_flag = 1;

static void exit_handler(int sig) { // can be called asynchronously
    run_flag = 0;                   // set flag
}

int main(int argc, char **argv) {
    char fifo_data[128];
    const int size = sizeof(fifo_data);
    const int timeout_sec = 10; // 10sec

    fd_set fds;
    struct timeval timeout;
    int bytes_read = 0;
    int ret;
    int n;
    int i, j;
    const int gestureThreshold = 30;
    int _detectedGesture = GESTURE_NONE;
    bool _gestureIn = false;
    int _gestureDirInX = 0;
    int _gestureDirInY = 0;
    int _gestureDirectionX = 0;
    int _gestureDirectionY = 0;
    const int _gestureSensitivity = 20;

    // printf ("Hello World, size of long int: %zd\n", sizeof (long int));

    // Register signals
    signal(SIGINT, exit_handler);

    // refer APDS9960::handleGesture

    int fd = open("/dev/iio:device0", O_RDONLY);

    if (fd != -1) {
        while (run_flag) {
            bytes_read = 0;
            memset(fifo_data, 0, size);

            FD_ZERO(&fds);
            FD_SET(fd, &fds);
            timeout.tv_sec = timeout_sec;
            timeout.tv_usec = 0;
            ret = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
            if (ret == 1) {
                n = read(fd, &fifo_data[bytes_read], size - bytes_read);
                bytes_read += n;
#if DEBUG
                printf("total read size:%d \n", bytes_read);
#endif

                for (i = 0; i + 3 < bytes_read; i += 4) {
                    uint8_t u, d, l, r;
                    u = fifo_data[i];
                    d = fifo_data[i + 1];
                    l = fifo_data[i + 2];
                    r = fifo_data[i + 3];

                    if ((u < gestureThreshold) && (d < gestureThreshold) &&
                        (l < gestureThreshold) && (r < gestureThreshold)) {

#if DEBUG
                        printf("u, ");
                        printf("d, ");
                        printf("l, ");
                        printf("r, ");
                        printf("\n");

                        // print the raw data
                        for (j = 0; j < bytes_read; j++) {
                            printf("%03d ", fifo_data[j]);

                            if (j % 4 == 3)
                                printf("\n");
                        }
#endif

                        _gestureIn = true;
                        if (_gestureDirInX != 0 || _gestureDirInY != 0) {
                            int totalX = _gestureDirInX - _gestureDirectionX;
                            int totalY = _gestureDirInY - _gestureDirectionY;
#if DEBUG
                            printf("OUT ");
                            printf("%d (%d - %d)", totalX, _gestureDirInX,
                                   _gestureDirectionX);
                            printf(", ");
                            printf("%d (%d - %d)", totalY, _gestureDirInY,
                                   _gestureDirectionY);
                            printf("\n");
#endif
                            if (totalX < -_gestureSensitivity) {
                                _detectedGesture = GESTURE_LEFT;
                                printf("left\n");
                            }
                            if (totalX > _gestureSensitivity) {
                                _detectedGesture = GESTURE_RIGHT;
                                printf("right\n");
                            }
                            if (totalY < -_gestureSensitivity) {
                                _detectedGesture = GESTURE_DOWN;
                                printf("down\n");
                            }
                            if (totalY > _gestureSensitivity) {
                                _detectedGesture = GESTURE_UP;
                                printf("up\n");
                            }
                            _gestureDirectionX = 0;
                            _gestureDirectionY = 0;
                            _gestureDirInX = 0;
                            _gestureDirInY = 0;
                        }
                        continue;
                    }

                    _gestureDirectionX = r - l;
                    _gestureDirectionY = u - d;
                    if (_gestureIn) {
                        _gestureIn = false;
                        _gestureDirInX = _gestureDirectionX;
                        _gestureDirInY = _gestureDirectionY;
#if DEBUG
                        printf("IN ");
                        printf("%d", _gestureDirInX);
                        printf(", ");
                        printf("%d", _gestureDirInY);
                        printf("\n");
#endif
                    }
                }
            }
        }

        close(fd);
        return 0;

    } else {
        printf("open fail \n");
        return -1;
    }
}