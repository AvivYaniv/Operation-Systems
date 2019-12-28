#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h> 
#include <string.h>
#include <errno.h>
#include <signal.h>

/**
 * @about 	Homework 3 - Operation Systems
 * @author 	Aviv Yaniv  
  */

/* Define Section */
/* Char Devices */
#define DEVICE_PREFIX									"/dev/"
#define DEVICE_NAME_MESSAGES_SLOT                       "message_slot"        

/* Arguments */
#define CHANNEL_FORMAT									"%u"
#define MESSAGE_FORMAT									"%s"

#define MESSAGE_MAX_LENGTH								128

// The major device number.
// We don't rely on dynamic registration
// any more. We want ioctls to know this
// number at compile time.
#define MAJOR_NUM                                       244

// Set the message of the device driver
#define MSG_SLOT_CHANNEL                                _IOW(MAJOR_NUM, 0, unsigned long)

/* Enums Section */
typedef enum BOOL
{ 
	FALSE 	=	0, 
	TRUE	=	1,
} BOOL;

typedef enum eArgumentIndexs
{ 
	MESSAGE_SLOT_FILE_PATH		=	1, 
	TARGET_MESSAGE_CHANNEL_ID	=	2, 
	NUMBER_OF_ARGUMENTS
} eArgumentIndexs;

/* Debug */
#define DEBUG											0
#if DEBUG
#define PRINT_DEBUG_MESSAGES
#endif

#define PRINT_BASIC_MESSAGES

/* Debug Messages */
#define DEVICE_OPENED_FRMT			                    "%s Device opened!\n"
#define DEVICE_NOT_OPENED_FRMT			                "%s Device not opened!\n"

#define FAILED_TO_PARSE_CHANNEL_FRMT					"Failed to parse channel: [%s] \n"
#define FAILED_CHANGE_CHANNEL_ID						" Failed to change channel id\n"

#define SET_CHANNEL_ID_FRMT								"Set channel id: %u\n"

#define FAILED_TO_READ_MESSAGE							"Failed to read message\n"

#define MESSAGE_READ_FROM_DEVICE_SUCCESSFULLY			"Message read from device successfully\n"

/* Methods Section */
/* 
 * Prints a buffer: all first length given characters, including '\0's
 * 
 * Arguments:
 * 		arrBuffer 	– Buffer to print
 * 		nLength 	– The amount of first characters to print
 * 
 * Exit value: None
 */
void printBuffer(char* arrBuffer, const int nLength)
{
	/* Variable Definition */
	int i = 0;

	/* Code Section */
	for(i=0; nLength > i ; ++i)
	{
		printf("%c",arrBuffer[i]);
	}

	printf("\n");
}

/* 
 * Read message from the message slot
 * 
 * Arguments:
 * 		argv[1] – message slot file path.
 * 		argv[2] – the target message channel id. Assume a non-negative integer.
 * 
 * Exit value: should be 0 on success and a non-zero value on error.
 */
int main(int argc, char *argv[])
{
	/* Variable Definition */
	int 			nReturnValue	=	0;
	int 			fdMessageSlot	=	0;
	unsigned int	uChannelId		= 	0;
	char			arrBuffer[MESSAGE_MAX_LENGTH + 1];

	/* Code Section */
	/* Validating arguments number */
	if (NUMBER_OF_ARGUMENTS != argc)
	{
		return EXIT_FAILURE;
	}

	/* Fetching channel id */
	if (1 != sscanf(argv[TARGET_MESSAGE_CHANNEL_ID], CHANNEL_FORMAT, &uChannelId))
	{
		#ifdef PRINT_BASIC_MESSAGES
			printf(FAILED_TO_PARSE_CHANNEL_FRMT, argv[TARGET_MESSAGE_CHANNEL_ID]);
		#endif

		/* Exit failure */
		exit(EXIT_FAILURE);
	}

	/* 1. Open the specified message slot device file. */
	fdMessageSlot = open(argv[MESSAGE_SLOT_FILE_PATH], O_RDONLY);

	/* Validating opening successully */
	if (0 > fdMessageSlot)
	{
		#ifdef PRINT_BASIC_MESSAGES
			printf(DEVICE_NOT_OPENED_FRMT, argv[MESSAGE_SLOT_FILE_PATH]);
		#endif

		/* Exit failure */
		exit(EXIT_FAILURE);
	}

	#ifdef PRINT_DEBUG_MESSAGES
		printf(DEVICE_OPENED_FRMT, argv[MESSAGE_SLOT_FILE_PATH]);
	#endif

	/* 2. Set the channel id to the id specified on the command line. */
	nReturnValue = ioctl(fdMessageSlot, MSG_SLOT_CHANNEL, uChannelId);

	/* Validating set successully */
	if (0 > nReturnValue)
	{
		#ifdef PRINT_BASIC_MESSAGES
			printf(FAILED_CHANGE_CHANNEL_ID);
		#endif

		/* Closing the device */
		close(fdMessageSlot);

		/* Exit failure */
		exit(EXIT_FAILURE);
	}
	
	#ifdef PRINT_DEBUG_MESSAGES
		printf(SET_CHANNEL_ID_FRMT, uChannelId);
	#endif

	/* 3. Read a message from the device to a buffer. */
	nReturnValue = read(fdMessageSlot, arrBuffer, MESSAGE_MAX_LENGTH);

	/* Validating read successully */
	if (0 > nReturnValue)
	{
		#ifdef PRINT_BASIC_MESSAGES
			printf(FAILED_TO_READ_MESSAGE);
		#endif

		/* Closing the device */
		close(fdMessageSlot);

		/* Exit failure */
		exit(EXIT_FAILURE);
	}

	/* 4. Close the device. */
	close(fdMessageSlot);

	/* If message length exceeds max length, should never get here */
	if (MESSAGE_MAX_LENGTH < nReturnValue)
	{
		#ifdef PRINT_BASIC_MESSAGES
			printf(FAILED_TO_READ_MESSAGE);
		#endif

		/* Exit failure */
		exit(EXIT_FAILURE);
	}

	/* Null-terminating the message before print */
	arrBuffer[nReturnValue] = '\0';

	/* 5. Print the message and a status message. */
	printBuffer(arrBuffer, nReturnValue);
	printf(MESSAGE_READ_FROM_DEVICE_SUCCESSFULLY);
	
	/* Return success */
	return EXIT_SUCCESS;
}
