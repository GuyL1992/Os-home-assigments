#ifndef MESSAGESLOT_H
#define MESSAGESLOT_H
#define SUCCESS 0
#define DEVICE_RANGE_NAME "message_slot"
#define DEVICE_FILE_NAME "message_slot_device"

#include <linux/ioctl.h>
#define MAJOR_NUMBER 235
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUMBER, 0, unsigned int)
#define MAX_MESSAGE_LEN 128

#endif


