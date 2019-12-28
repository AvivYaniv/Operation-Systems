#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <errno.h>

/**
 * @about 	Homework 5 - Operation Systems
 * @code	Client Side
 * @author 	Aviv Yaniv  
  */

/* Define Section */
/* Debug */
#define DEBUG 0
#if DEBUG
#define DEBUG_SERVER_IP 							"127.0.0.1"
#define DEBUG_SERVER_PORT 							"999"
#define DEBUG_LENGTH  								"999999"
#define PRINT_DEBUG_MESSAGES
#endif

/* Arguments */
#define NUMBER_FORMAT 								"%d"
#define MESSAGE_FORMAT 								"%s"

/* Types Strings Section */
#define UINT32_SIZE									sizeof(uint32_t)

/* Buffer Section */
#define SEND_BUFFER_LENGTH							1024 /* Precautions over content size, 1400 is maximal TCP Based on: https://stackoverflow.com/questions/2613734/maximum-packet-size-for-a-tcp-connection */

/* Data to send Section */
#define DATA_FILE_TO_SEND							"/dev/urandom"

/* I/O Section */
#define IO_OPERATION_FAILURE						-1

/* Enums Section */
typedef enum BOOL 
{
	FALSE 						= 0,
	TRUE 						= 1,
} BOOL;

typedef enum eArgumentIndexs 
{
	SERVER_IP 					= 1,
	SERVER_PORT 				= 2,
	LENGTH 						= 3,
	NUMBER_OF_ARGUMENTS
} eArgumentIndexs;

#define PRINT_BASIC_MESSAGES

/* Debug Messages */
#define READ_FROM_FILE_MSG_FRMT 					"Read from file: %s\n"
#define SENT_TO_SERVER_MSG_FRMT 					"Sent to server: %s\n"
#define SENT_SERVER_MESSAGE_LENGTH_MSG_FRMT			"Sent server message length: %u\n"

/* Basic Messages */
#define NUMBER_OF_PRINTABLE_CHARACTERS_FRMT 		"# of printable characters: %u\n"
#define CLIENT_SOCKET_CREATED_FRMT 					"Client: socket created %s:%d\n"
#define	CLIENT_CONNECTING_MSG 						"Client: connecting...\n"
#define	CLIENT_CONNECTED_DETAILS_FRMT				"Client: Connected. \n" "\t\tSource IP: %s Source Port: %d\n" "\t\tTarget IP: %s Target Port: %d\n"

/* Error Messages */
#define INVALID_NUMBER_OF_ARGUMENTS_ERR_MSG	 		"Invalid number of arguments.\n"
#define COULD_NOT_CREATE_SOCKET_ERR_MSG 		 	"\n Error : Could not create socket \n"
#define FAILED_ALLOCATE_SEND_BUFFER_ERR_MSG 	 	"Failed allocate send buffer.\n"
#define CONNECT_FAILED_ERR_MSG_FRMT					"\n Error : Connect Failed. %s \n"
#define FAILED_OPEN_DATA_SEND_FILE_ERR_MSG_FRMT		"Failed to open data send file. %s\n"
#define FAILED_WRITE_BUFFER_TO_FILE_ERR_MSG_FRMT	"Failed write buffer to file. %s\n"
#define FAILED_READ_BUFFER_FROM_FILE_ERR_MSG_FRMT	"Failed read buffer from file. %s\n"
#define FAILED_READ_DATA_SEND_FILE_ERR_MSG_FRMT		"Failed read data send file. %s\n"
#define FAILED_WRITE_DATA_LENGTH_ERR_MSG			"Failed write data length to server.\n"
#define FAILED_WRITE_DATA_TO_SERVER_ERR_MSG_FRMT	"Failed write data to server. %s\n"
#define FAILED_READ_DATA_FROM_SERVER_ERR_MSG_FRMT	"Failed read data from server. %s\n"

/* Static Definition */

/* Methods Section */
/* 
 * Reads to buffer from a file.
 * 
 * Arguments:
 * 		fdFile 	– File descriptor to read from (already opened)
 * 		uLength – Length to read from file
 * 		pBuffer – Poiter to buffer
 * 
 * Return value: Number of bytes read, or IO_OPERATION_FAILURE upon failure
 */
int readAllBuffer(const int fdFile, void* pBuffer, const unsigned int uLength)
{
	/* Variable Section */
	unsigned int 	uTotal 		= 0;
	int				nRead 		= 0;

	/* Code Section */
	/* As long as there are bytes to read */
	while (uLength > uTotal)
	{
		/* Read from file */
		nRead = read(fdFile, pBuffer + uTotal, uLength - uTotal);

		/* If end of file */
		if (0 == nRead)
		{
			/* Return value */
			return uTotal;
		}
		/* Else, if failed to read */
		else if (0 > nRead)
		{
			#ifdef PRINT_DEBUG_MESSAGES
				/* Printing error message */
				fprintf(stderr, FAILED_READ_BUFFER_FROM_FILE_ERR_MSG_FRMT, strerror(errno));		
			#endif

			/* Return failure */
			return IO_OPERATION_FAILURE;
		}

		/* Updating total bytes read */
		uTotal += nRead;
	}

	/* Return value */
	return uTotal;
}

/* 
 * Writes buffer to a file.
 * 
 * Arguments:
 * 		fdFile 	– File descriptor to write to (already opened)
 * 		uLength – Length to write from buffer
 * 		pBuffer – Poiter to buffer
 * 
 * Return value: Number of bytes written, or negative IO_OPERATION_FAILURE upon failure
 */
int writeAllBuffer(const int fdFile, void* pBuffer, unsigned int uLength)
{
	/* Variable Section */
	unsigned int 		uTotal 		= 0;
	int					nWritten 	= 0;

	/* Code Section */
	/* As long as there are bytes to write */
	while (uLength > uTotal)
	{
		/* Write to buffer */
		nWritten = write(fdFile, pBuffer + uTotal, uLength - uTotal);

		/* If end of file */
		if (0 == nWritten)
		{
			/* Return value */
			return uTotal;
		}
		/* Else, if failed to write */
		else if (0 > nWritten)
		{
			#ifdef PRINT_DEBUG_MESSAGES
				/* Printing error message */
				fprintf(stderr, FAILED_WRITE_BUFFER_TO_FILE_ERR_MSG_FRMT, strerror(errno));		
			#endif

			/* Return failure */
			return IO_OPERATION_FAILURE;
		}

		/* Updating total bytes written */
		uTotal += nWritten;
	}

	/* Return value */
	return uTotal;
}

/* 
 * The client creates a TCP connection to the server and sends it N bytes read from /dev/urandom, 
 * where N is a user-supplied argument. 
 * The client then reads back the count of printable characters from the server, prints it, and exits.
 * 
 * Arguments:
 * 		argv[1] – Server IP. 								(Assume it is valid IP address)
 * 		argv[2] – Server port.								(Assume it is 16-bit integer)
 * 		argv[3] – The number of bytes to send to server.	(Assume it is an unsigned integer)
 * 
 * Exit value: 
 * 		EXIT_SUCCESS	-	On success.
 * 		EXIT_FAILURE	-	On failure.
 */
int main(int argc, char *argv[])
{
	/* Struct Definition */
	struct sockaddr_in 	serv_addr; 				/* where we Want to get to */
	struct sockaddr_in 	my_addr;   				/* where we actually connected through  */
	struct sockaddr_in 	peer_addr; 				/* where we actually connected to */
	
	/* Array Definition */
	char				read_send_buff[SEND_BUFFER_LENGTH];

	/* Variable Definition */
	BOOL				bIsEndOfTransmission	= FALSE;

	/* Socket */
	socklen_t 			addrsize 				= sizeof(struct sockaddr_in );
	int  				fdSocket   				= -1;

	/* File */
	int  				fdFile     				= -1;
	int					nNowRead				= 0;
	unsigned int		uToRead					= 0;

	/* User Parameters */
	char* 				strServerIP 			= NULL;
	unsigned short		usPort					= 0;
	char* 				strLength 				= NULL;
	unsigned int		uLength					= 0;

	/* Send Message */	
	int					nNowSent				= 0;
	unsigned int		uLeftToSend				= 0;
	uint32_t			uMessageLength			= 0;

	/* Recive Message */
	uint32_t			uPrintableCount			= 0;
	unsigned int		C 						= 0;

	/* Code Section */
	#ifdef DEBUG
		argc 				= NUMBER_OF_ARGUMENTS;
		argv[SERVER_IP]		= DEBUG_SERVER_IP;
		argv[SERVER_PORT] 	= DEBUG_SERVER_PORT;
		argv[LENGTH] 		= DEBUG_LENGTH;
	#endif

	/* Validating arguments number */
	if (NUMBER_OF_ARGUMENTS != argc)
	{
		#ifdef PRINT_BASIC_MESSAGES
			/* Printing error message */
			fprintf(stderr, INVALID_NUMBER_OF_ARGUMENTS_ERR_MSG);		
		#endif

		/* Exit failure */
		return EXIT_FAILURE;
	}

	/* Setting the server IP */
	strServerIP = argv[SERVER_IP];

	/* Setting server port */
	usPort = (unsigned short)atoi(argv[SERVER_PORT]);

	/* Setting length */
	strLength = argv[LENGTH];
	uLength = (unsigned int)strtoul(strLength, 0L, 10);

	/* 1. Create a TCP connection to the specified server port on the specified server ip. */
	/* Validating socket creation */
	if (0 > (fdSocket = socket(AF_INET, SOCK_STREAM, 0)))
	{
		#ifdef PRINT_BASIC_MESSAGES
			/* Printing error message */
			fprintf(stderr, COULD_NOT_CREATE_SOCKET_ERR_MSG);		
		#endif

		/* Exit failure */
		return EXIT_FAILURE;
	}
	
	/* Get our socket name */
	getsockname(fdSocket, (struct sockaddr*) &my_addr, &addrsize);
	
	#ifdef PRINT_DEBUG_MESSAGES
		printf(CLIENT_SOCKET_CREATED_FRMT, inet_ntoa((my_addr.sin_addr)), ntohs(my_addr.sin_port));		
	#endif 

	/* Setting server address */
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(usPort);
	serv_addr.sin_addr.s_addr = inet_addr(strServerIP); 

	#ifdef PRINT_DEBUG_MESSAGES
		printf(CLIENT_CONNECTING_MSG);
	#endif 

	/* connect socket to the target address */
	if(0 > connect(fdSocket,
				   (struct sockaddr*) &serv_addr,
				   sizeof(serv_addr)))
	{
		#ifdef PRINT_BASIC_MESSAGES
			/* Printing error message */			
			printf(CONNECT_FAILED_ERR_MSG_FRMT, strerror(errno));			
		#endif

		/* Exit failure */
		return EXIT_FAILURE;
	}

	#ifdef PRINT_DEBUG_MESSAGES
		getsockname(fdSocket, (struct sockaddr*) &my_addr,   &addrsize);
		getpeername(fdSocket, (struct sockaddr*) &peer_addr, &addrsize);
		printf(CLIENT_CONNECTED_DETAILS_FRMT,
			   inet_ntoa((my_addr.sin_addr)),    ntohs(my_addr.sin_port),
			   inet_ntoa((peer_addr.sin_addr)),  ntohs(peer_addr.sin_port));			   
	#endif 

	/* 2. Open /dev/urandom for reading. */
	/* Open file data to send */
	if (0 > (fdFile = open(DATA_FILE_TO_SEND, O_RDONLY)))
	{
		#ifdef PRINT_BASIC_MESSAGES
			/* Printing error message */			
			fprintf(stderr, FAILED_OPEN_DATA_SEND_FILE_ERR_MSG_FRMT, strerror(errno));			
		#endif

		/* Closing socket & file */
		close(fdFile);
		close(fdSocket);

		/* Exit failure */
		return EXIT_FAILURE;
	}

	/* 3. Transfer the specified length bytes from /dev/urandom to the server over the TCP connection. */
	/* Creating the header - which contains the length of the message, in a network-safe matter */
	uMessageLength = htonl(uLength);

	/* Writing to server the message header, amount of bytes to be sent */
	if (UINT32_SIZE > writeAllBuffer(fdSocket, &uMessageLength, UINT32_SIZE))
	{
		#ifdef PRINT_BASIC_MESSAGES
			/* Printing error message */			
		fprintf(stderr, FAILED_WRITE_DATA_LENGTH_ERR_MSG);			
		#endif

		/* Closing socket & file */
		close(fdFile);
		close(fdSocket);

		/* Exit failure */
		return EXIT_FAILURE;
	}

	#ifdef PRINT_DEBUG_MESSAGES
			printf(SENT_SERVER_MESSAGE_LENGTH_MSG_FRMT, uLength);
		#endif

	/* Initializing left to send */
	uLeftToSend = uLength;
		
	/* While have data to read and send to server */
	while ((0 < uLeftToSend) && !bIsEndOfTransmission)
	{
		/* Adjust read amount by send buffer length */
		uToRead = (SEND_BUFFER_LENGTH > uLeftToSend) ? uLeftToSend : SEND_BUFFER_LENGTH;

		/* Read from file to send buffer */
		if (0 > (nNowRead = readAllBuffer(fdFile, &read_send_buff, uToRead)))
		{
			#ifdef PRINT_BASIC_MESSAGES
				/* Printing error message */
				fprintf(stderr, FAILED_READ_DATA_SEND_FILE_ERR_MSG_FRMT, strerror(errno));		
			#endif

			/* Closing socket & file */
			close(fdFile);
			close(fdSocket);

			/* Exit failure */
			return EXIT_FAILURE;
		}

		#ifdef PRINT_DEBUG_MESSAGES
			read_send_buff[nNowRead] = '\0';
			printf(READ_FROM_FILE_MSG_FRMT, read_send_buff);
		#endif

		/* Send from buffer to server */
		if (0 > (nNowSent = writeAllBuffer(fdSocket, &read_send_buff, nNowRead)))
		{
			#ifdef PRINT_BASIC_MESSAGES
				/* Printing error message */
				fprintf(stderr, FAILED_WRITE_DATA_TO_SERVER_ERR_MSG_FRMT, strerror(errno));		
			#endif

			/* Closing socket & file */
			close(fdFile);
			close(fdSocket);

			/* Exit failure */
			return EXIT_FAILURE;
		}

		/* Flagging end of transmission */
		bIsEndOfTransmission = (0 == nNowSent) ? TRUE : FALSE;

		#ifdef PRINT_DEBUG_MESSAGES
			read_send_buff[nNowSent] = '\0';
			printf(SENT_TO_SERVER_MSG_FRMT, read_send_buff);						
		#endif

		/* Updating left to send */
		uLeftToSend -= nNowSent;
	}

	/* Closing file */
	close(fdFile);

	/* 4. Obtain the count of printable characters computed by the server, C (an unsigned int), and print it to the user in the following manner: */
	/* Receive from server the count of printable characters */
	if (UINT32_SIZE > (nNowRead = readAllBuffer(fdSocket, &uPrintableCount, UINT32_SIZE)))
	{
		#ifdef PRINT_BASIC_MESSAGES
			/* Printing error message */
			fprintf(stderr, FAILED_READ_DATA_FROM_SERVER_ERR_MSG_FRMT, strerror(errno));		
		#endif

		/* Closing socket  */
		close(fdSocket);

		/* Exit failure */
		return EXIT_FAILURE;
	}

	/* Closing socket */
	close(fdSocket);

	/* Translate count from network to host */
	C = ntohl(uPrintableCount);

	/* printf("# of printable characters: %u\n", C); */
	printf(NUMBER_OF_PRINTABLE_CHARACTERS_FRMT, C);
	
	/* 5. Exit with code 0. */
	return EXIT_SUCCESS;
}
