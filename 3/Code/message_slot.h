#ifndef _MESSAGE_SLOT_H_
#define _MESSAGE_SLOT_H_

#include <linux/ioctl.h>
#include <linux/list.h>

/* Define Section */
/* Char Devices */
#define MULTITHREADED                                   0
#if MULTITHREADED
#define IS_CONCURRENT
#endif

#define CHAR_FORMAT                                     "%c"

#define DEVICE_NAME_MESSAGES_SLOT                       "message_slot"        

// The major device number.
// We don't rely on dynamic registration
// any more. We want ioctls to know this
// number at compile time.
#define MAJOR_NUM                                       244

// Set the message of the device driver
#define MSG_SLOT_CHANNEL                                _IOW(MAJOR_NUM, 0, unsigned long)

/* Return Values */
#define SUCCESS                                         0

/* Message Slot */
#define INVALID_CHANNEL_ID                              0
#define MAX_CHANNEL_ID                                  4294967295u /* UINT_MAX */
#define MESSAGE_MAX_LENGTH                              128

#define END_OF_MESSAGE                                  '\0'

/* Enum Section */
typedef enum { FALSE, TRUE } BOOL;

/*
 * Message device holds 
 * - an open device minor number
 * - devices current channel defined by ioctl
 */
typedef struct opened_device_info_t {
    unsigned int        device_minor_number;
    unsigned int        current_channel;

	struct list_head    opened_devices_list;	        // devices kernel list
} opened_device_info_t;

/*
 * Message slot holds 
 * - specific device minor number and channel number
 * - message held in that message slot
 */
typedef struct message_slot_t {
    unsigned int        device_minor_number;
    unsigned int        channel;

    unsigned int        length;
    char                buffer[MESSAGE_MAX_LENGTH + 1];

	struct list_head    message_slot_list;	            // message slot kernel list
} message_slot_t;

/* Prints */
#define PRINT                                           1
#if PRINT
    #define PRINT_BASIC    
#endif

/* Messages */
#ifdef PRINT_BASIC
/* Info */
#define MESSAGE_SLOT_REGISTERED_MAJOR_NUMBER_FRMT       "message_slot: registered major number %d\n"

/* Error */
#define MESSAGE_SLOT_REGISTERING_FAILED_MSG             "Message slot registering failed\n"
#endif

/* Debug */
#define DEBUG											0
#if DEBUG
#define PRINT_DEBUG_MESSAGES
#define PRINT_INSTRUCTIONS
#endif

#ifdef PRINT_INSTRUCTIONS
/* Instructions */
#define HOW_TO_USE_DRIVER_INSTRUCTIONS_FRMT             "Registeration is successful. " "The major device number is %d.\n" "If you want to talk to the device driver,\n" "you have to create a device file:\n" "mknod /dev/%s c %d 0\n" "You can echo/cat to/from the device file.\n" "Dont forget to rm the device file and " "rmmod when you're done\n"
#endif

/* Debug Messages */
#define ADDED_NEW_DEVICE_FRMT                           "Added Device=%u\n"

#define ADDED_NEW_MESSAGE_SLOT_FRMT                     "Added message slot Device=%u Chanel=%u\n"
#define READ_MESSAGE_SLOT_FRMT                          "Read message slot Device=%u Chanel=%u\n"
#define WRITE_MESSAGE_SLOT_FRMT                         "Write message slot Device=%u Chanel=%u\n"
#define DEVICE_IS_NOT_OPENED_ERROR                      "Device %u is not opened!\n"

#define MEMORY_ALLOCATION_FAILED_MSG                    "Memory allocation failed!\n"
#define USER_BUFFER_IS_INSUFFICIENT 					"User buffer is insufficient!\n"
#define DEVICE_ALREADY_OPENED_FRMT                      "Device %u already opened!\n"

#define DEVICE_INIT_BEGIN						        " Device init begin!\n"
#define DEVICE_INIT_FAILED						        " Device init failed!\n"
#define DEVICE_INIT_ENDED						        " Device init end!\n"

#define DEVICE_OPENED_D_TIMES_FRMT						" Device opened %d times.\n"

#define DEVICE_EXIT_BEGIN						        " Device exit begin!\n"
#define DEVICE_EXIT_FAILED						        " Device exit failed!\n"
#define DEVICE_EXIT_ENDED						        " Device exit end!\n"

#define DEVICE_READ_BEGIN								" Device read begin!\n"
#define DEVICE_READ_ENDED								" Device read end!\n"
#define DEVICE_READ_FINISHED							" Device read finished!\n"

#define DEVICE_WRITE_BEGIN								" Device write begin!\n"
#define DEVICE_WRITE_ENDED								" Device write end!\n"
#define DEVICE_WRITE_FINISHED							" Device write finished!\n"

#define DEVICE_PARSED_DATA_ALREADY_EXISTS               " Device parsed data already exists!\n"
#define DEVICE_PARSING_FRMT                             " Device parsing: %s\n"
#define DEVICE_PARSED_SUCCESSFULLY                      " Device parsed successfully\n"

#define DEVICE_WRITE_FINISHED_NO_DATA_ERROR				" Device write finished no data!\n"
#define DEVICE_FAILED_PARSE_USER_CODE_ERROR				" Device failed parse user code!\n"
#define DEVICE_FAILED_PARSE_DATA_ERROR			        " Device failed parse data!\n"
#define DEVICE_RECIVED_TOO_MUCH_DATA_ERROR			    " Device recived too much data!\n"

#define DEVICE_IOCTL_COMMAND_IS_INVALID_ERROR   		" Device ioctl %u command is invalid!\n"
#define DEVICE_IOCTL_PARAMETER_IS_INVALID_ERROR   		" Device ioctl %lu parameter is invalid!\n"

#define DEVICE_RELEASED_D_TIMES_FRMT					" Device released %d times.\n"

#define DEVICE_DATA_CLEARED                             " Device data cleared.\n"

#endif
