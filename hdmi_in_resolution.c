#include <fcntl.h>
#include <linux/videodev2.h>
#include <signal.h> // signal functions
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

typedef unsigned char uint8_t;

volatile sig_atomic_t run_flag = 1;

static void exit_handler(int sig) { // can be called asynchronously
    run_flag = 0;                   // set flag
}

int main(int argc, char **argv) {
    int result;
    struct v4l2_event event;

    // Register signals
    signal(SIGINT, exit_handler);
    int fd = open("/dev/v4l-subdev0", O_RDWR);

    if (fd < 0) {
        printf("open fail \n");
        return -1;
    } else {

        // Subscribe to the resolution change event.
        struct v4l2_event_subscription sub;
        memset(&sub, 0, sizeof(sub));
        sub.type = V4L2_EVENT_SOURCE_CHANGE;
        if (ioctl(fd, /*VIDIOC_SUBSCRIBE_EVENT*/ 1075861082, &sub) == -1) {
            printf("ioctl() failed: VIDIOC_SUBSCRIBE_EVENT: V4L2_EVENT_SOURCE_CHANGE \n");
            goto end;
        } else {
            printf("ioctl() ok: VIDIOC_SUBSCRIBE_EVENT: V4L2_EVENT_SOURCE_CHANGE \n");
        }


        while (run_flag) {
            memset(&event, 0, sizeof(struct v4l2_event));
            printf("ioctl VIDIOC_DQEVENT \n");
            result = ioctl(fd, /*VIDIOC_DQEVENT*/ -2138548647, &event);
            if (result == 0) {
                printf("onDequeueEvent type=%d \n", event.type);
                if (event.type == V4L2_EVENT_SOURCE_CHANGE) {
                    if (event.u.src_change.changes & V4L2_EVENT_SRC_CH_RESOLUTION) {
                        // handleFormatChanged();
                        printf("resolution change \n");
                    }
                }
            } else {
                printf("ioctl fail %d \n", result);
            }

            sleep(1);
        }

    end:
        close(fd);
        return 0;
    }
}