#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <errno.h>

/**
 * @about 	Homework 5 - Operation Systems
 * @code	Server Side
 * @author 	Aviv Yaniv  
 */

/* Define Section */
/* Debug */
#define DEBUG 0
#if DEBUG
#define DEBUG_SERVER_PORT 							"999"
#define PRINT_DEBUG_MESSAGES
#endif

/* Arguments */
#define NUMBER_FORMAT 								"%d"
#define MESSAGE_FORMAT 								"%s"

/* Types Strings Section */
#define UINT32_SIZE									sizeof(uint32_t)

/* Printable Chars */
#define LOWEST_PRINTABLE_ASCII_CODE					32
#define HIGHEST_PRINTABLE_ASCII_CODE				126
#define PRINTABLE_ASCII_RANGE						(HIGHEST_PRINTABLE_ASCII_CODE - LOWEST_PRINTABLE_ASCII_CODE + 1)
#define CHAR_TO_HISTOGRAM(C)						(C - LOWEST_PRINTABLE_ASCII_CODE)
#define HISTOGRAM_TO_CHAR(H)						(H + LOWEST_PRINTABLE_ASCII_CODE)

/* Chars Histogram */
#define CHARS_HISTOGRAM_SIZE						PRINTABLE_ASCII_RANGE

/* Server Queue Listen */
#define SERVER_QUEUE_LISTEN_SIZE					10

/* Buffer Section */
#define RECIVE_BUFFER_LENGTH						1024 /* Precautions over content size, 1400 is maximal TCP Based on: https://stackoverflow.com/questions/2613734/maximum-packet-size-for-a-tcp-connection */

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
	SERVER_PORT 				= 1,
	NUMBER_OF_ARGUMENTS
} eArgumentIndexs;

#define PRINT_BASIC_MESSAGES

/* Debug Messages */
#define CLIENT_MESSAGE_LENGTH_MSG_FRMT				"Client message length: %u\n"
#define RECIVED_FROM_CLIENT_MSG_FRMT		 		"Recived from client: %s\n"
#define SENT_CLIENT_COUNTER_MSG_FRMT				"Sent client counter: %u\n"

/* Basic Messages */
#define SERVER_ACCEPTED_CLIENT_DETAILS_FRMT 		"Server: Client connected.\n" "\t\tClient IP: %s Client Port: %d\n" "\t\tServer IP: %s Server Port: %d\n"

/* Server Histogram */
#define CHAR_APPEARED_TIMES_MSG				 		"char '%c' : %u times\n"

/* Error Messages */
#define INVALID_NUMBER_OF_ARGUMENTS_ERR_MSG	 		"Invalid number of arguments.\n"
#define SERVER_BIND_ERR_MSG_FRMT					"\n Error : Bind Failed. %s \n"
#define SERVER_LISTEN_FAILED_ERR_MSG_FRMT			"\n Error : Listen Failed. %s \n"
#define CLIENT_ACCEPT_ERR_MSG_FRMT					"\n Error : Accept Failed. %s \n"
#define FAILED_TO_REGISTER_SIGINT_ERR_MSG	 		"Failed to register SIGINT handler.\n"
#define FAILED_READ_DATA_LENGTH_ERR_MSG				"Failed read data length from client.\n"
#define FAILED_WRITE_BUFFER_TO_FILE_ERR_MSG_FRMT	"Failed write buffer to file. %s\n"
#define FAILED_WRITE_CLIENT_DATA_ERR_MSG_FRMT		"Failed write client data.\n"
#define FAILED_READ_BUFFER_FROM_FILE_ERR_MSG_FRMT	"Failed read buffer from file. %s\n"
#define FAILED_READ_DATA_FROM_CLIENT_ERR_MSG_FRMT	"Failed read data from client. %s\n"

/* Static Definition */
/* Server */
static BOOL 		s_bRunServer;
static BOOL 		s_bCanStopServer;

/* Chars Histogram : Index indicates char (offset LOWEST_PRINTABLE_ASCII_CODE), value is count */
static unsigned int s_arrGlobalCharsHistogram[CHARS_HISTOGRAM_SIZE];

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
 * Reads a stream of bytes from the client, 
 * compute its printable character count and write the result to the client over the TCP connection. 
 * Also updates the pcc_total global data structure.
 * 
 * Arguments:
 * 		fdClientSocket 	– File descriptor of client
 * 
 * Return value: None
 */
void handleClient(int fdClientSocket)
{
	/* Array Definition */
	char				recive_buff[RECIVE_BUFFER_LENGTH];

	/* Variable Definition */
	BOOL				bIsEndOfTransmission	= FALSE;
	char				cCurrentChar			= 0;
	int 				nBufferIndex			= 0;

	/* Chars Histogram : Index indicates char (offset LOWEST_PRINTABLE_ASCII_CODE), value is count */
	unsigned int 		arrClientCharsHistogram[CHARS_HISTOGRAM_SIZE];
	int					nHistogramIndex			= 0;

	/* Send Message */	
	uint32_t			uPrintableCount			= 0;
	uint32_t			uPrintableCountNetwork	= 0;

	/* Recive Message */
	unsigned int		uLength					= 0;
	uint32_t			uMessageLengthFromNet	= 0;
	unsigned int		uLeftToRecive			= 0;
	unsigned int		uToRead					= 0;
	int					nNowRecived				= 0;	
	
	/* Code Section */
	/* 4A. If the server is in the middle of processing a client, finish computing their printable character counts and update the pcc_total global data structure. */
	/* Flagging server to not be stopped, as we're hadnling a client here! */
	s_bCanStopServer = FALSE;

	/* Initialize client chars histogram */
	for (nHistogramIndex = 0; CHARS_HISTOGRAM_SIZE > nHistogramIndex; ++nHistogramIndex)
	{
		arrClientCharsHistogram[nHistogramIndex] = 0;
	}

	/* Reads a stream of bytes from the client */
	/* Read from client the message header, amount of bytes to be recived */
	if (UINT32_SIZE > readAllBuffer(fdClientSocket, &uMessageLengthFromNet, UINT32_SIZE))
	{
		#ifdef PRINT_BASIC_MESSAGES
			/* Printing error message */			
		fprintf(stderr, FAILED_READ_DATA_LENGTH_ERR_MSG);
		#endif

		/* Close client socket */
		close(fdClientSocket);

		/* Return */
		return;
	}

	/* Translating message length network to host */
	uLength = ntohl(uMessageLengthFromNet);

	#ifdef PRINT_DEBUG_MESSAGES
		printf(CLIENT_MESSAGE_LENGTH_MSG_FRMT, uLength);
	#endif

	/* Initializing left to recive */
	uLeftToRecive = uLength;

	/* Initializing printable count */
	uPrintableCount = 0;

	/* As long as there is data to recive */
	while ((0 < uLeftToRecive) && !bIsEndOfTransmission)
	{
		/* Adjust read amount by read buffer length */
		uToRead = (RECIVE_BUFFER_LENGTH > uLeftToRecive) ? uLeftToRecive : RECIVE_BUFFER_LENGTH;

		/* Read from client */
		if (0 > (nNowRecived = readAllBuffer(fdClientSocket, &recive_buff, uToRead)))
		{
			#ifdef PRINT_BASIC_MESSAGES
				/* Printing error message */
			fprintf(stderr, FAILED_READ_DATA_FROM_CLIENT_ERR_MSG_FRMT, strerror(errno));		
			#endif

			/* Closing client socket */
			close(fdClientSocket);

			/* Return */
			return;
		}
		
		/* Flagging end of transmission */
		bIsEndOfTransmission = (0 == nNowRecived) ? TRUE : FALSE;

		#ifdef PRINT_DEBUG_MESSAGES
			recive_buff[nNowRecived] = '\0';
			printf(RECIVED_FROM_CLIENT_MSG_FRMT, recive_buff);
		#endif

		/* Compute its printable character count */
		/* Going over the bytes from buffer, updating chars histogram */
		for (nBufferIndex = 0; nBufferIndex < nNowRecived; ++nBufferIndex)
		{
			/* Setting current char */
			cCurrentChar = recive_buff[nBufferIndex];

			/* If current char is printable */
			if ((HIGHEST_PRINTABLE_ASCII_CODE 	>= 	cCurrentChar) && 
			 	(LOWEST_PRINTABLE_ASCII_CODE 	<= 	cCurrentChar))
			{
				/* Updating client chars histogram */
				++arrClientCharsHistogram[CHAR_TO_HISTOGRAM(cCurrentChar)];

				/* Updating printable count */
				++uPrintableCount;
			}
		}		

		/* Updating left to recive */
		uLeftToRecive -= nNowRecived;
	}

	/* Write the result to the client over the TCP connection. */
	uPrintableCountNetwork = htonl(uPrintableCount);

	/* Writing to client the printable count */
	if (UINT32_SIZE > writeAllBuffer(fdClientSocket, &uPrintableCountNetwork, UINT32_SIZE))
	{
		#ifdef PRINT_BASIC_MESSAGES
			/* Printing error message */			
		fprintf(stderr, FAILED_WRITE_CLIENT_DATA_ERR_MSG_FRMT);			
		#endif
	}

	#ifdef PRINT_DEBUG_MESSAGES
		printf(SENT_CLIENT_COUNTER_MSG_FRMT, uPrintableCount);		
	#endif

 	/* Also updates the pcc_total global data structure */
	for (nHistogramIndex = 0; CHARS_HISTOGRAM_SIZE > nHistogramIndex; ++nHistogramIndex)
	{
		s_arrGlobalCharsHistogram[nHistogramIndex] +=
			arrClientCharsHistogram[nHistogramIndex];
	}	

	/* Close client socket */
	close(fdClientSocket);

	/* Flagging server can be stopped, as we're hadnling a client here! */
	s_bCanStopServer = TRUE;
}

/* 
 * Stopping server and exitting
 * 
 * Arguments: None
 * 
 * Exit value: None
 */
void stopServer()
{
	/* Variable Definition */
	int					nHistogramIndex			= 0;

	/* Code Section */
	/* 4. If the user hits Ctrl-C, perform the following actions: */
	/* 4B. Print out the number of times each printable character was observed by clients. */	
	for (nHistogramIndex = 0; CHARS_HISTOGRAM_SIZE > nHistogramIndex; ++nHistogramIndex)
	{
		/* The format of the printout is the following line, for each printable character: char '%c' : %u times\n */
		printf(CHAR_APPEARED_TIMES_MSG, HISTOGRAM_TO_CHAR(nHistogramIndex), s_arrGlobalCharsHistogram[nHistogramIndex]);
	}	
	
	/* 4C. Exit with exit code 0. */
	exit(EXIT_SUCCESS);
}

/* 
 * Flagging server to stop ASAP (now or after finishing current client)
 * 
 * Arguments:
 * 		sig – Signal number.
 * 
 * Exit value: None
 */
void sigint_handler(int sig)
{
	/* Code Section */
	/* 4. If the user hits Ctrl-C, perform the following actions: */	
	/* If server can be stopped */
	if (s_bCanStopServer)
	{
		/* Stopping server */
		stopServer();
	}
	/* Else, flagging server to stop running when possible */	
	else
	{
		/* Flagging server to stop running */
		s_bRunServer = FALSE;
	}
}

/* 
 * The server accepts TCP connections from clients. 
 * A client that connects sends the server a stream of N bytes (N depends on the client, and is not a global constant). 
 * The server counts the number of printable characters in the stream (a printable character is a byte whose value is in the range [32,126]). 
 * Once the stream ends, the server sends the count back to the client over the same connection.
 * In addition, the server maintains a data structure in which it counts the number of times each printable character was observed in all the connections. When the server receives a SIGINT, it prints these counts and exits.
 * 
 * Arguments:
 * 		argv[1] – Server port.								(Assume it is 16-bit integer)
 * 
 * Exit value: 
 * 		EXIT_SUCCESS	-	On success.
 * 		EXIT_FAILURE	-	On failure.
 */
int main(int argc, char *argv[])
{
	/* Struct Definition */
	struct sockaddr_in serv_addr;
  	struct sockaddr_in my_addr;
  	struct sockaddr_in peer_addr;
	
	/* Variable Definition */	
	/* Socket */
	socklen_t 			addrsize 				= sizeof(struct sockaddr_in );
	int  				fdListenSocket   		= -1;
	int  				fdClientSocket   		= -1;
	
	/* User Parameters */
	unsigned short		usPort					= 0;

	/* Chars Histogram */
	int					nHistogramIndex			= 0;

	/* Code Section */
	#ifdef DEBUG
		argc 				= NUMBER_OF_ARGUMENTS;
		argv[SERVER_PORT] 	= DEBUG_SERVER_PORT;
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

	/* Setting server port */
	usPort = (unsigned short)atoi(argv[SERVER_PORT]);

	/* 1. Initialize a data structure pcc_total that will count how many times each printable character was observed in all client connections. */
	/* Initialize global chars histogram */
	for (nHistogramIndex = 0; CHARS_HISTOGRAM_SIZE > nHistogramIndex; ++nHistogramIndex)
	{
		s_arrGlobalCharsHistogram[nHistogramIndex] = 0;
	}

	/* Configuring to run server */
	s_bRunServer 		= TRUE;
	s_bCanStopServer	= TRUE;

	/* Setting SIGNIT handler to be able to stop server */
	if ((SIG_ERR == signal(SIGINT, sigint_handler)))
	{
		#ifdef PRINT_BASIC_MESSAGES
			/* Printing error message */			
			fprintf(stderr, FAILED_TO_REGISTER_SIGINT_ERR_MSG);
		#endif
		
		/* Exit failure */
		return EXIT_FAILURE;
	}
	
	/* 2. Listen to incoming TCP connections with queue of size 10 */
	fdListenSocket = socket( AF_INET, SOCK_STREAM, 0 );
	memset( &serv_addr, 0, addrsize );
	serv_addr.sin_family = AF_INET;
	/* INADDR_ANY = any local machine address */
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(usPort);

	/* Binding server to port and any client */
	if( 0 != bind( fdListenSocket,
					(struct sockaddr*) &serv_addr,
					addrsize ) )
	{
		#ifdef PRINT_BASIC_MESSAGES
			/* Printing error message */			
			fprintf(stderr, SERVER_BIND_ERR_MSG_FRMT, strerror(errno));
		#endif

		/* Exit failure */
		return EXIT_FAILURE;
	}

	/* Listen to incoming TCP connections with queue of size 10 */
	if( 0 != listen( fdListenSocket, SERVER_QUEUE_LISTEN_SIZE ) )
	{
		#ifdef PRINT_BASIC_MESSAGES
			/* Printing error message */			
			fprintf(stderr, SERVER_LISTEN_FAILED_ERR_MSG_FRMT, strerror(errno));
		#endif

		/* Exit failure */
		return EXIT_FAILURE;
	}

	/* 3. Enter a loop, in which you: */
	while ( s_bRunServer )
	{
		/* 3A. Accept a TCP connection on the specified server port. */
		/* Accept a connection. */
		fdClientSocket = accept( fdListenSocket,
								 (struct sockaddr*) &peer_addr,
								 &addrsize);

		/* Validating client was accepted */
		if ( 0 > fdClientSocket)
		{
			#ifdef PRINT_BASIC_MESSAGES
				/* Printing error message */			
				fprintf(stderr, CLIENT_ACCEPT_ERR_MSG_FRMT, strerror(errno));
			#endif

			/* Continue to handle another client */
			continue;
		}

		#ifdef PRINT_DEBUG_MESSAGES
			getsockname(fdClientSocket, (struct sockaddr*) &my_addr,   &addrsize);
			getpeername(fdClientSocket, (struct sockaddr*) &peer_addr, &addrsize);
			printf( SERVER_ACCEPTED_CLIENT_DETAILS_FRMT,
					inet_ntoa( peer_addr.sin_addr ),
					ntohs(     peer_addr.sin_port ),
					inet_ntoa( my_addr.sin_addr   ),
					ntohs(     my_addr.sin_port   ) );
		#endif

		/* 3B. When a connection is accepted, read a stream of bytes from the client, 
		 * compute its printable character count and write the result to the client over the TCP connection. 
		 * Also updates the pcc_total global data structure. */
		handleClient(fdClientSocket);
	}

	/* Stopping server */
	stopServer();

	/* 5. Exit with code 0. */
	return EXIT_SUCCESS;
}
