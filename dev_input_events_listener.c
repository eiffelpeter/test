#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/poll.h>
#include <linux/input.h>
#include <unistd.h>
#include <errno.h>
#include <linux/input-event-codes.h>

#include "dev_input_events_listener.h"

#define EVENT_FILES_DIR "/dev/input/"

int getFileDescriptorByEventHandler(char *eventHandler) {

    // Construct the path to the event file: /dev/input/eventX
    char eventFile[256];

    // Find the position of the newline character in eventHandler
    char *newlinePos = strchr(eventHandler, '\n');
    size_t length = 0;
    if (newlinePos != NULL) {
        // Calculate the length up to the newline character
        length = newlinePos - eventHandler;
    } else {
        // If no newline character is found, use the whole string
        length = strlen(eventHandler);
    }

    // Copy up to the newline character into a temporary buffer
    char tempEventHandler[256];
    strncpy(tempEventHandler, eventHandler, length);
    tempEventHandler[length] = '\0'; // Null-terminate the string

    // Construct the full path
    snprintf(eventFile, sizeof(eventFile), "%s%s", EVENT_FILES_DIR, tempEventHandler);

printf("open event file %s\n", eventFile);

    // Open the event file
    int fd = open(eventFile, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open event file");
        return -1;
    }
    return fd;
}

void listen_dev_input_events(char *eventHandlers, ProcessEventCallback processEventCallback) {

    struct pollfd *pollingRequests = NULL;  // Array of file descriptors to monitor
    size_t numberOfFileDescriptors = 0;     // Number of file descriptors to monitor


    {

        // Find the file descriptor for the event handler
        int fd = getFileDescriptorByEventHandler(eventHandlers);

        // If the file descriptor is valid
        if (fd != -1) {
        	
            numberOfFileDescriptors++;

            // Reallocate memory for pollingRequests
            struct pollfd *temp = realloc(pollingRequests, numberOfFileDescriptors * sizeof(struct pollfd));
            if (temp == NULL) {
                printf("Failed to reallocate memory for polling requests\n");
                for (size_t i = 0; i < numberOfFileDescriptors - 1; i++) {
                    if (pollingRequests[i].fd != -1) {
                        close(pollingRequests[i].fd);
                    }
                }
                free(pollingRequests);
                return;
            }
            pollingRequests = temp;

            // Add the file descriptor to the 'pollingRequests' array
            pollingRequests[numberOfFileDescriptors - 1].fd = fd;
            pollingRequests[numberOfFileDescriptors - 1].events = POLLIN; // Input events when there is data to read
        } else {
        	printf("listen_dev_input_events fd -1 \n");
        }

    }

    // Monitor the file descriptors for events
    if (pollingRequests != NULL) {

        // Monitor the file descriptors for events
        int running = 1;
        while (running) {
            // Poll the file descriptors for events
            int numberOfReadyFDs = poll(pollingRequests, numberOfFileDescriptors, -1);
            if (numberOfReadyFDs == -1) {
                perror("poll");
                break;
            }

            // Loop through the file descriptors and check for events
            for (size_t i = 0; i < numberOfReadyFDs; i++) {
                if (pollingRequests[i].revents & POLLIN) {

                    // Read the event from the file descriptor
                    struct input_event ev;
                    ssize_t bytesRead = read(pollingRequests[i].fd, &ev, sizeof(ev));

                    // Check if the read operation was successful
                    if (bytesRead == -1) { // Error reading from file descriptor
                        fprintf(stderr, "Error reading from file descriptor %d: %s\n", pollingRequests[i].fd, strerror(errno));
                    } else if (bytesRead == 0) { // End of file, close the file descriptor
                    }

                    // Check if the event is set
                    if (bytesRead == sizeof(ev)) {
                        // Process the input event
                        // If the processEventCallback returns 0, stop the event loop

                        running = 1;
                        printf("[%ld.%ld] keycode %d (%s) \n", ev.time.tv_sec, ev.time.tv_usec/1000, ev.code, (ev.value) ? "down" : "up");

                    }


                    // Check if all file descriptors have been closed
                    bool allClosed = true;
                    for (size_t i = 0; i < numberOfReadyFDs; i++) {
                        if (pollingRequests[i].fd != -1) {
                            allClosed = false;
                            break;
                        }
                    }
                    if (allClosed) {
                        break;
                    }

                }
            }
        }
    }

    // Free the allocated memory for pollingRequests and close file descriptors
    for (size_t i = 0; i < numberOfFileDescriptors; i++) {
        if (pollingRequests[i].fd != -1) {
            close(pollingRequests[i].fd);
        }
    }
    free(pollingRequests);
}


int main() {

    listen_dev_input_events("event1", NULL );

    return EXIT_SUCCESS;
}
