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

#define READ_FIFO_TIMEOUT_MS 30 // after check scope, the next interrupt usually come in 20ms.
                                // so we define 30ms as timeout, assume hand leave and no more fifo data
#define GESTURE_THRESHOLD_OUT 10
#define GESTURE_SENSITIVITY_1 50
#define GESTURE_SENSITIVITY_2 20

typedef unsigned char uint8_t;

/* Direction definitions */
enum { DIR_NONE, DIR_LEFT, DIR_RIGHT, DIR_UP, DIR_DOWN, DIR_NEAR, DIR_FAR, DIR_ALL };

/* State definitions */
enum { NA_STATE, NEAR_STATE, FAR_STATE, ALL_STATE };
/* Container for gesture data */
typedef struct gesture_data_type {
    uint8_t u_data[32];
    uint8_t d_data[32];
    uint8_t l_data[32];
    uint8_t r_data[32];
    uint8_t index;
    uint8_t total_gestures;
    uint8_t in_threshold;
    uint8_t out_threshold;
} gesture_data_type;

gesture_data_type gesture_data_;
int gesture_ud_delta_;
int gesture_lr_delta_;
int gesture_ud_count_;
int gesture_lr_count_;
int gesture_near_count_;
int gesture_far_count_;
int gesture_state_;
int gesture_motion_;

volatile sig_atomic_t run_flag = 1;

static void exit_handler(int sig) { // can be called asynchronously
    run_flag = 0;                   // set flag
}

/**
 * @brief Resets all the parameters in the gesture data member
 */
void resetGestureParameters() {
    gesture_data_.index = 0;
    gesture_data_.total_gestures = 0;

    gesture_ud_delta_ = 0;
    gesture_lr_delta_ = 0;

    gesture_ud_count_ = 0;
    gesture_lr_count_ = 0;

    gesture_near_count_ = 0;
    gesture_far_count_ = 0;

    gesture_state_ = 0;
    gesture_motion_ = DIR_NONE;
}

/**
 * @brief Processes the raw gesture data to determine swipe direction
 *
 * @return True if near or far state seen. False otherwise.
 */
bool processGestureData() {
    uint8_t u_first = 0;
    uint8_t d_first = 0;
    uint8_t l_first = 0;
    uint8_t r_first = 0;
    uint8_t u_last = 0;
    uint8_t d_last = 0;
    uint8_t l_last = 0;
    uint8_t r_last = 0;
    int ud_ratio_first;
    int lr_ratio_first;
    int ud_ratio_last;
    int lr_ratio_last;
    int ud_delta;
    int lr_delta;
    int i;

    /* If we have less than 4 total gestures, that's not enough */
    if (gesture_data_.total_gestures <= 4) {
        printf("less than 4 total gestures, that's not enough \n");
        return false;
    }

    /* Check to make sure our data isn't out of bounds */
    if ((gesture_data_.total_gestures <= 32) && (gesture_data_.total_gestures > 0)) {

        /* Find the first value in U/D/L/R above the threshold */
        for (i = 0; i < gesture_data_.total_gestures; i++) {
            if ((gesture_data_.u_data[i] > GESTURE_THRESHOLD_OUT) && (gesture_data_.d_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.l_data[i] > GESTURE_THRESHOLD_OUT) && (gesture_data_.r_data[i] > GESTURE_THRESHOLD_OUT)) {

                u_first = gesture_data_.u_data[i];
                d_first = gesture_data_.d_data[i];
                l_first = gesture_data_.l_data[i];
                r_first = gesture_data_.r_data[i];
                break;
            }
        }

        /* If one of the _first values is 0, then there is no good data */
        if ((u_first == 0) || (d_first == 0) || (l_first == 0) || (r_first == 0)) {
            // printf("one of the _first values is 0, then there is no good data \n");
            return false;
        }
        /* Find the last value in U/D/L/R above the threshold */
        for (i = gesture_data_.total_gestures - 1; i >= 0; i--) {
#if DEBUG
            printf("Finding last: \n");
            printf("U:");
            printf("%d\n", gesture_data_.u_data[i]);
            printf(" D:");
            printf("%d\n", gesture_data_.d_data[i]);
            printf(" L:");
            printf("%d\n", gesture_data_.l_data[i]);
            printf(" R:");
            printf("%d\n", gesture_data_.r_data[i]);
#endif
            if ((gesture_data_.u_data[i] > GESTURE_THRESHOLD_OUT) && (gesture_data_.d_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.l_data[i] > GESTURE_THRESHOLD_OUT) && (gesture_data_.r_data[i] > GESTURE_THRESHOLD_OUT)) {

                u_last = gesture_data_.u_data[i];
                d_last = gesture_data_.d_data[i];
                l_last = gesture_data_.l_data[i];
                r_last = gesture_data_.r_data[i];
                break;
            }
        }
    }

    /* Calculate the first vs. last ratio of up/down and left/right */
    ud_ratio_first = ((u_first - d_first) * 100) / (u_first + d_first);
    lr_ratio_first = ((l_first - r_first) * 100) / (l_first + r_first);
    ud_ratio_last = ((u_last - d_last) * 100) / (u_last + d_last);
    lr_ratio_last = ((l_last - r_last) * 100) / (l_last + r_last);

#if DEBUG
    printf("Last Values: ");
    printf("U:");
    printf("%d\n", u_last);
    printf(" D:");
    printf("%d\n", d_last);
    printf(" L:");
    printf("%d\n", l_last);
    printf(" R:");
    printf("%d\n", r_last);

    printf("Ratios: ");
    printf("UD Fi: ");
    printf("%d\n", ud_ratio_first);
    printf(" UD La: ");
    printf("%d\n", ud_ratio_last);
    printf(" LR Fi: ");
    printf("%d\n", lr_ratio_first);
    printf(" LR La: ");
    printf("%d\n", lr_ratio_last);
#endif
    /* Determine the difference between the first and last ratios */
    ud_delta = ud_ratio_last - ud_ratio_first;
    lr_delta = lr_ratio_last - lr_ratio_first;

#if DEBUG
    printf("Deltas: ");
    printf("UD: ");
    printf("%d\n", ud_delta);
    printf(" LR: ");
    printf("%d\n", lr_delta);
#endif

    /* Accumulate the UD and LR delta values */
    gesture_ud_delta_ += ud_delta;
    gesture_lr_delta_ += lr_delta;

#if DEBUG
    printf("Accumulations: ");
    printf("UD: ");
    printf("%d\n", gesture_ud_delta_);
    printf(" LR: ");
    printf("%d\n", gesture_lr_delta_);
#endif

    /* Determine U/D gesture */
    if (gesture_ud_delta_ >= GESTURE_SENSITIVITY_1) {
        gesture_ud_count_ = 1;
    } else if (gesture_ud_delta_ <= -GESTURE_SENSITIVITY_1) {
        gesture_ud_count_ = -1;
    } else {
        gesture_ud_count_ = 0;
    }

    /* Determine L/R gesture */
    if (gesture_lr_delta_ >= GESTURE_SENSITIVITY_1) {
        gesture_lr_count_ = 1;
    } else if (gesture_lr_delta_ <= -GESTURE_SENSITIVITY_1) {
        gesture_lr_count_ = -1;
    } else {
        gesture_lr_count_ = 0;
    }

    /* Determine Near/Far gesture */
    if ((gesture_ud_count_ == 0) && (gesture_lr_count_ == 0)) {
        if ((abs(ud_delta) < GESTURE_SENSITIVITY_2) && (abs(lr_delta) < GESTURE_SENSITIVITY_2)) {

            if ((ud_delta == 0) && (lr_delta == 0)) {
                gesture_near_count_++;
            } else if ((ud_delta != 0) || (lr_delta != 0)) {
                gesture_far_count_++;
            }

            if ((gesture_near_count_ >= 10) && (gesture_far_count_ >= 2)) {
                if ((ud_delta == 0) && (lr_delta == 0)) {
                    gesture_state_ = NEAR_STATE;
                } else if ((ud_delta != 0) && (lr_delta != 0)) {
                    gesture_state_ = FAR_STATE;
                }
                return true;
            }
        }
    } else {
        if ((abs(ud_delta) < GESTURE_SENSITIVITY_2) && (abs(lr_delta) < GESTURE_SENSITIVITY_2)) {

            if ((ud_delta == 0) && (lr_delta == 0)) {
                gesture_near_count_++;
            }

            if (gesture_near_count_ >= 10) {
                gesture_ud_count_ = 0;
                gesture_lr_count_ = 0;
                gesture_ud_delta_ = 0;
                gesture_lr_delta_ = 0;
            }
        }
    }

#if DEBUG
    printf(" UD_CT: ");
    printf("%d\n", gesture_ud_count_);
    printf(" LR_CT: ");
    printf("%d\n", gesture_lr_count_);
    printf(" NEAR_CT: ");
    printf("%d\n", gesture_near_count_);
    printf(" FAR_CT: ");
    printf("%d\n", gesture_far_count_);
    printf("----------\n");
#endif

    // printf("processGestureData return fail \n");
    return false;
}

/**
 * @brief Determines swipe direction or near/far state
 *
 * @return True if near/far event. False otherwise.
 */
bool decodeGesture() {
    /* Return if near or far event is detected */
    if (gesture_state_ == NEAR_STATE) {
        gesture_motion_ = DIR_NEAR;
        return true;
    } else if (gesture_state_ == FAR_STATE) {
        gesture_motion_ = DIR_FAR;
        return true;
    }

    /* Determine swipe direction */
    if ((gesture_ud_count_ == -1) && (gesture_lr_count_ == 0)) {
        gesture_motion_ = DIR_UP;
    } else if ((gesture_ud_count_ == 1) && (gesture_lr_count_ == 0)) {
        gesture_motion_ = DIR_DOWN;
    } else if ((gesture_ud_count_ == 0) && (gesture_lr_count_ == 1)) {
        gesture_motion_ = DIR_RIGHT;
    } else if ((gesture_ud_count_ == 0) && (gesture_lr_count_ == -1)) {
        gesture_motion_ = DIR_LEFT;
    } else if ((gesture_ud_count_ == -1) && (gesture_lr_count_ == 1)) {
        if (abs(gesture_ud_delta_) > abs(gesture_lr_delta_)) {
            gesture_motion_ = DIR_UP;
        } else {
            gesture_motion_ = DIR_RIGHT;
        }
    } else if ((gesture_ud_count_ == 1) && (gesture_lr_count_ == -1)) {
        if (abs(gesture_ud_delta_) > abs(gesture_lr_delta_)) {
            gesture_motion_ = DIR_DOWN;
        } else {
            gesture_motion_ = DIR_LEFT;
        }
    } else if ((gesture_ud_count_ == -1) && (gesture_lr_count_ == -1)) {
        if (abs(gesture_ud_delta_) > abs(gesture_lr_delta_)) {
            gesture_motion_ = DIR_UP;
        } else {
            gesture_motion_ = DIR_LEFT;
        }
    } else if ((gesture_ud_count_ == 1) && (gesture_lr_count_ == 1)) {
        if (abs(gesture_ud_delta_) > abs(gesture_lr_delta_)) {
            gesture_motion_ = DIR_DOWN;
        } else {
            gesture_motion_ = DIR_RIGHT;
        }
    } else {
        // printf("decodeGesture return fail \n");
        return false;
    }

    switch (gesture_motion_) {
    case DIR_UP:
        printf("UP\n");
        break;
    case DIR_DOWN:
        printf("DOWN\n");
        break;
    case DIR_LEFT:
        printf("LEFT\n");
        system("input keyevent 4");  // back
        break;
    case DIR_RIGHT:
        printf("RIGHT\n");
        system("input keyevent 3");  // home
        break;
    case DIR_NEAR:
        printf("NEAR\n");
        break;
    case DIR_FAR:
        printf("FAR\n");
        break;
    default:
        printf("NONE\n");
    }
    return true;
}

int enable_apds9960_gesture(const char *pathname, bool en) {
    int fd = 0;
    char buf[1];
    int size;

    buf[0] = en ? '1' : '0';

    fd = open(pathname, O_RDWR);
    if (fd != -1) {
        size = write(fd, &buf[0], sizeof(buf));

        if (size != sizeof(buf)) {
            printf("write %s size is %d \n", pathname, size);
        }
        close(fd);
    } else {
        printf("open %s fail \n", pathname);
    }
}

int main(int argc, char **argv) {
    char fifo_data[128];
    const int size = sizeof(fifo_data);

    fd_set fds;
    struct timeval timeout;
    int bytes_read = 0;
    int ret;
    int n;
    int i;

    enable_apds9960_gesture("/sys/bus/iio/devices/iio:device0/scan_elements/in_proximity1_en", 1);
    enable_apds9960_gesture("/sys/bus/iio/devices/iio:device0/scan_elements/in_proximity2_en", 1);
    enable_apds9960_gesture("/sys/bus/iio/devices/iio:device0/scan_elements/in_proximity3_en", 1);
    enable_apds9960_gesture("/sys/bus/iio/devices/iio:device0/scan_elements/in_proximity4_en", 1);
    enable_apds9960_gesture("/sys/bus/iio/devices/iio:device0/buffer/enable", 1);

    // printf ("Hello World, size of long int: %zd\n", sizeof (long int));

    // Register signals
    signal(SIGINT, exit_handler);

    // refer APDS9960::handleGesture

    int fd = open("/dev/iio:device0", O_RDONLY);

    gesture_data_.index = 0;
    gesture_data_.total_gestures = 0;

    if (fd != -1) {
        while (run_flag) {
            bytes_read = 0;
            memset(fifo_data, 0, size);
            do {
                FD_ZERO(&fds);
                FD_SET(fd, &fds);
                timeout.tv_sec = 0;
                timeout.tv_usec = READ_FIFO_TIMEOUT_MS * 1000;
                ret = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
                if (ret == 1) {
                    n = read(fd, &fifo_data[bytes_read], size - bytes_read);
                    bytes_read += n;
                }
            } while (bytes_read < size && ret == 1);

            if (bytes_read) {
#if DEBUG
                printf("total read size:%d \n", bytes_read);

                // print the raw data
                for (i = 0; i < bytes_read; i++) {
                    printf("%02X ", fifo_data[i]);

                    if (i % 4 == 3)
                        printf("\n");
                }
#endif

                /* If at least 1 set of data, sort the data into U/D/L/R */
                if (bytes_read >= 4) {
                    for (i = 0; i < bytes_read; i += 4) {
                        gesture_data_.u_data[gesture_data_.index] = fifo_data[i + 0];
                        gesture_data_.d_data[gesture_data_.index] = fifo_data[i + 1];
                        gesture_data_.l_data[gesture_data_.index] = fifo_data[i + 2];
                        gesture_data_.r_data[gesture_data_.index] = fifo_data[i + 3];
                        gesture_data_.index++;
                        gesture_data_.total_gestures++;
                    }

#if DEBUG
                    printf("Up Data: \n");
                    for (i = 0; i < gesture_data_.total_gestures; i++) {
                        printf("%02X ", gesture_data_.u_data[i]);

                        if (i % 4 == 3)
                            printf("\n");
                    }
                    printf("\n");
#endif

                    /* Filter and process gesture data. Decode near/far state */
                    if (processGestureData()) {
                        if (decodeGesture()) {
                            //***TODO: U-Turn Gestures
#if DEBUG
                            // Serial.println(gesture_motion_);
#endif
                        }
                    }

                    /* Reset data */
                    gesture_data_.index = 0;
                    gesture_data_.total_gestures = 0;

                    bytes_read = 0;
                }
            } else {

                /* Determine best guessed gesture and clean up */
                decodeGesture();
#if DEBUG
                Serial.print("END: ");
                Serial.println(gesture_motion_);
#endif
                resetGestureParameters();
#if DEBUG
                printf("total read size:%d beacuse:%s\n", bytes_read, ret == 0 ? "timeour" : "error");
#endif
            }
        }

        enable_apds9960_gesture("/sys/bus/iio/devices/iio:device0/buffer/enable", 0);
        close(fd);
        return 0;
    } else {
        printf("open fail \n");
        return -1;
    }
}