#ifndef MESSAGESLOT_H
#define MESSAGESLOT_H

#include <linux/ioctl.h>

// The major device number.
// We don't rely on dynamic registration
// any more. We want ioctls to know this
// number at compile time.
//#define MAJOR_NUM 244

// Set the message of the device driver
#define IOCTL_SET_ENC _IOW(MAJOR_NUM, 0, unsigned long)

// #define DEVICE_RANGE_NAME "char_dev"
// #define BUF_LEN 80
// #define DEVICE_FILE_NAME "simple_char_dev"
// #define SUCCESS 0

#define MAJOR_NUMBER 11
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUMBER, 0, unsigned int)
#define MAX_MESSAGE_LEN 128

#endif

