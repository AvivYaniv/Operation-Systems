#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>

/**
 * @about 	Homework 4 - Operation Systems
 * @author 	Aviv Yaniv  
  */

/* Define Section */
/* Arguments */
#define NUMBER_FORMAT 			"%d"
#define MESSAGE_FORMAT 			"%s"

/* Enums Section */
typedef enum BOOL {
	FALSE 						= 0,
	TRUE 						= 1,
} BOOL;

typedef enum eArgumentIndexs {
	SEARCH_ROOT_DIRECTORY 		= 1,
	SEARCH_TERM 				= 2,
	NUMBER_OF_THREADS 			= 3,
	NUMBER_OF_ARGUMENTS
} eArgumentIndexs;

#define FOREACH_THREAD_STATES(THREAD_STATE) \
            THREAD_STATE(THREAD_STATE_IDLE) \
			THREAD_STATE(THREAD_STATE_WORKING) \
            THREAD_STATE(THREAD_STATE_DONE) \

#ifndef ENUM_GENERATORS
	#define ENUM_GENERATORS
	#define GENERATE_ENUM(ENUM) 	ENUM,
	#define GENERATE_STRING(STRING) #STRING,
#endif

typedef enum thread_state_t {
    FOREACH_THREAD_STATES(GENERATE_ENUM)
} thread_state_t;

/* Debug */
#define DEBUG 0
#if DEBUG
#define PRINT_DEBUG_MESSAGES
#endif

#define PRINT_BASIC_MESSAGES

/* Debug Messages */
#define FAILED_TO_PARSE_NUMBER_OF_THREADS_FRMT 		"Failed to parse number of threads: %s \n"
#define FAILED_TO_INIT_CONCURRENCY					"Failed to init concurrency\n"
#define FAILED_TO_DESTROY_CONCURRENCY				"Failed to destroy concurrency\n"

#define FAILED_TO_ALLOCATE_THREADS_ARRAY			"Failed to allocate threads array\n"
#define FAILED_CREATING_THREAD_FRMT					"Failed creating thread: %s\n"
#define FAILED_TO_SET_CANCEL_HANDLER				"Failed to set cancel handler.\n"

#define CURRENT_THREAD_IS_FRMT						"Current thread is: %lu \n"

#define FAILED_TO_ALLOCATE_MEMORY					"Failed to allocate memory.\n"
#define FAILED_TO_READ_DIRECTORY					"Failed to read directory.\n"

#define CANCELING_THREAD_FRMT						"Cancelling thread %d.\n"
#define JOINING_THREAD_FRMT							"Joining thread %d.\n"
#define ACCEPTED_CANCELATION_REQUEST				"Accepted cancellation request\n"

#define CONCAT_PATH_FRMT							"%s/%s"
#define FILE_MATCH_SEARCH_FRMT						"%s\n"

#define THREAD_CANT_OPEN_DIRECTORY_FRMT				"Thread %lu cant open directory %s! \n"
#define THREAD_DONE_SEARCH_ITERATION_FRMT			"Thread %lu done search iteration! \n"

#define THREAD_IS_ERRORNEOUS_FRMT					"Thread %d is errorneous %s \n"

#define SEARCH_DONE_FRMT							"Done searching, found %d files\n"
#define SEARCH_STOPPED_FRMT							"Search stopped, found %d files.\n"

#ifdef PRINT_DEBUG_MESSAGES
    static const char* THREAD_STATES_STRING[] = {        
		FOREACH_THREAD_STATES(GENERATE_STRING)
	};
#endif

/* Struct Section */
typedef struct thread_with_status_t
{
	long			result;
	pthread_t  		thread_id;
	thread_state_t	thread_state;
} thread_with_status_t;

/* Queue Section */
/*
 * Based on: https://gist.github.com/kroggen/5fc7380d30615b2e70fcf9c7b69997b6
 */
typedef struct node
{
	char *val;
	struct node *next;
} node_t;

BOOL enqueue(node_t **head, char *val)
{
	node_t *new_node = (node_t *)calloc(1, sizeof(node_t));
	if (!new_node)
	{
		return FALSE;
	}

	new_node->val = val;
	new_node->next = *head;

	*head = new_node;

	return TRUE;
}

char* dequeue(node_t** head)
{
	node_t* current 	= NULL;
	node_t* prev 		= NULL;
	char *retval 		= NULL;

	if (*head == NULL)
	{
		return NULL;
	}

	current = *head;
	while (current->next != NULL)
	{
		prev = current;
		current = current->next;
	}

	retval = current->val;
	free(current);

	if (prev)
	{
		prev->next = NULL;
	}
	else
	{
		*head = NULL;
	}

	return retval;
}

void clear_queue(node_t *head)
{
	char* c;
	while (head)
	{
		if ((c = dequeue(&head)))
		{
			free(c);
		}
	}
}

#ifdef PRINT_DEBUG_MESSAGES
void print_queue(node_t *head)
{
	node_t *current = head;
	
	printf("Queue:\n");

	if (current == NULL)
	{
		printf("[Empty]\n");
		return;
	}

	while (current != NULL)
	{
		printf("%s\n", (char*)current->val);
		current = current->next;
	}
}
#endif

/* Static Definition */
static char* 					s_strSearchTerm 	= NULL;

/* Queue Decleration Section */
static node_t* 					s_pQueueDirectories = NULL;

/* Concurrency Decleration Section */
static thread_with_status_t* 	s_threads;
static int 						s_nNumberOfThreads	= 0;
static pthread_mutex_t 			s_queue_mutex_lock;
static pthread_cond_t  			s_queue_coinditional;

/* Methods Section */
/* 
 * Exitting if whole search is done:
 * 		- Work queue is empty
 * 		- No working threads
 * Work queue mutex is unlocked before exit (supposing it was locked)
 * 
 * Arguments:
 * 		tsCurrentThread 	– Current thread status and params
 * 		lCurrentThreadIndex - Current thread index
 * 
 * Return value: None
 */
void exit_if_whole_search_done(thread_with_status_t* 	tsCurrentThread, 
						 	   long 					lCurrentThreadIndex)
{
	/* Variable Definition */
	BOOL			bIsAnyWorking			=	FALSE;
	int 			nThreadIndex			= 	0;

	/* Code Section */
	/* If queue is empty */
	if (!s_pQueueDirectories)
	{
		/* Setting no other working before testing */
		bIsAnyWorking = FALSE;

		/* Going over the threads searching for working thread */
		for (nThreadIndex = 0; s_nNumberOfThreads > nThreadIndex; ++nThreadIndex)
		{
			/* If found working thread */
			if ((nThreadIndex != lCurrentThreadIndex) &&
				(THREAD_STATE_WORKING == s_threads[nThreadIndex].thread_state))
			{
				/* Setting still at work */
				bIsAnyWorking = TRUE;

				/* Ending loop */
				break;
			}
		}

		/* If none working - broadcasting to finish */
		if (!bIsAnyWorking)
		{
			/* Setting thread state as done */
			tsCurrentThread->thread_state = THREAD_STATE_DONE;

			/* Broadcasting others to stop waiting */
			pthread_cond_broadcast(&s_queue_coinditional);

			/* Unlocking queue mutex */
			pthread_mutex_unlock(&s_queue_mutex_lock);

			/* Return success */
			pthread_exit(NULL);
		}
	}
}

void cancel_theads_handler(int sig) 
{
	/* Variable Definition */
	void*     				pStatus			= NULL;
	int 					nTotalFound		= 0;
	int 					nThreadIndex 	= 0;
	int 					nReturnValue  	= EXIT_SUCCESS;

	/* Code Section */
	#ifdef PRINT_DEBUG_MESSAGES
		printf(ACCEPTED_CANCELATION_REQUEST);
	#endif

	/* Canceling each and every thread */
	for (nThreadIndex = 0; s_nNumberOfThreads > nThreadIndex; ++nThreadIndex)
	{
		#ifdef PRINT_DEBUG_MESSAGES
			printf(CANCELING_THREAD_FRMT, nThreadIndex);
		#endif

		/* Canceling thread */
		pthread_cancel(s_threads[nThreadIndex].thread_id);
	}

	/* Waking up threads */
	pthread_cond_broadcast(&s_queue_coinditional);

	/* Initializing match counter */
	nTotalFound = 0;

	/* 4. When there are no more directories in the queue, 
	 * 	  and all threads are idle (not searching for content within a directory) 
	 * 	  the program should exit with exit code 0, 
	 *    and print to stdout how many matching files were found. 
	 *    (printf(“Done searching, found %d files”, num_found)) 
	 */
	for (nThreadIndex = 0; s_nNumberOfThreads > nThreadIndex; ++nThreadIndex)
	{
		#ifdef PRINT_DEBUG_MESSAGES
			printf(JOINING_THREAD_FRMT, nThreadIndex);
		#endif

		/* Waiting for thread to finish */
		nReturnValue = pthread_join(s_threads[nThreadIndex].thread_id, &pStatus);

		/* Adding result to match counter */
		nTotalFound += s_threads[nThreadIndex].result;

		/* Validatig thread creation */
        if (nReturnValue)
        {
			#ifdef PRINT_BASIC_MESSAGES
            	fprintf(stderr, THREAD_IS_ERRORNEOUS_FRMT, nThreadIndex, strerror(nReturnValue));
			#endif

            /* Return failure */
			nReturnValue = EXIT_FAILURE;
        }
	}

	/* Free queue */
	clear_queue(s_pQueueDirectories);

	/* Free threads array */
	free(s_threads);

	#ifdef PRINT_BASIC_MESSAGES
		printf(SEARCH_STOPPED_FRMT, nTotalFound);
	#endif
	
	/* Exit program */
	exit(nReturnValue);
}

/* 
 * Search for term in file names, by single thread
 * 
 * Arguments:
 * 		thread_param 		– Current thread index
 * 
 * Return value: Nothing is actually being returned
 * 				 Result is updated in thread status
 */
void* thread_search(void* thread_param)
{
    /* Variable Definition */	
	long 			lCurrentThreadIndex 	= 	(long)thread_param;
    struct dirent*	entry 					= 	NULL;
    DIR* 			dir 					= 	NULL;
	char* 			path					= 	NULL;
	char* 			full_path				= 	NULL;	

	/* Code Section */
	/* Fetching current thread */
	thread_with_status_t* tsCurrentThread = &s_threads[lCurrentThreadIndex];

	/* Initializing match counter */
	tsCurrentThread->result = 0;

	/* As long as there is work to do */
	while (TRUE)
	{
		/* Test cancel thread */
		pthread_testcancel();

		#ifdef PRINT_DEBUG_MESSAGES
			printf(CURRENT_THREAD_IS_FRMT, lCurrentThreadIndex);
		#endif

		/* 1. Dequeue one directory from the queue. If the queue is empty, sleep until it becomes non-empty. Do not busy wait (wasting CPU cycles) until the directory to become non-empty. */
		/* Locking queue mutex */
		pthread_mutex_lock(&s_queue_mutex_lock);

		/* Setting state as idle */
		tsCurrentThread->thread_state = THREAD_STATE_IDLE;
		
		/* While queue is empty */
		while (!s_pQueueDirectories)
		{	
			/* Test cancel thread */
			pthread_mutex_unlock(&s_queue_mutex_lock);
			pthread_testcancel();
			pthread_mutex_lock(&s_queue_mutex_lock);

			/* Exitting if whole search is done */
			exit_if_whole_search_done(tsCurrentThread, lCurrentThreadIndex);

			/* Waiting for queue to be not empty */
			pthread_cond_wait(&s_queue_coinditional, &s_queue_mutex_lock);

			/* Test cancel thread */
			pthread_mutex_unlock(&s_queue_mutex_lock);
			pthread_testcancel();
			pthread_mutex_lock(&s_queue_mutex_lock);
		}

		/* Dequeueing path from directories queue */
		path = dequeue(&s_pQueueDirectories);

		/* Setting state as working */
		tsCurrentThread->thread_state = THREAD_STATE_WORKING;

		/* Unlocking queue mutex */
		pthread_mutex_unlock(&s_queue_mutex_lock);

		/* Testing if have to cancel thread - before handling new directory */
		pthread_testcancel();

		/* Opening current directory */
		dir = opendir(path);

		/* Validating directory opening */
		if (!dir) 
		{			
			#ifdef PRINT_BASIC_MESSAGES
				fprintf(stderr, THREAD_CANT_OPEN_DIRECTORY_FRMT, tsCurrentThread->thread_id, path);
			#endif

			/* Freeing current directory path */
			free(path);		
			
			/* Continue in work */
			continue;
		}

		/* 2. Iterate through each file in that directory. */
		/* Going over directory entries */
		while ((entry = readdir(dir))) 
		{
			/* Testing if have to cancel thread - before allocating momory */
			pthread_testcancel();

			/* Validating entry */
			if (!entry)
			{
				#ifdef PRINT_BASIC_MESSAGES
					fprintf(stderr, FAILED_TO_READ_DIRECTORY);
				#endif 

				/* Return no memory */
				pthread_exit((void*)errno);
			}

			/* Handling entry according to it's type */
			switch (entry->d_type)
			{
				/* In case it's a file */
				case (DT_REG):
				{
					/* 3. If the file name contains the search term (argument 2, case sensitive), print the full path of that file (including the file’s name) to stdout (printf(“%s\n”, full_path)) */
					if (strstr(entry->d_name, s_strSearchTerm))
					{
						/* Updating match counter */
						++(tsCurrentThread->result);

						/* Creating full path */
						full_path = (char*)calloc(strlen(path) + strlen(entry->d_name) + 2, sizeof(char));

						/* Validating allocation */
						if (!full_path)
						{
							#ifdef PRINT_BASIC_MESSAGES
								fprintf(stderr, FAILED_TO_ALLOCATE_MEMORY);
							#endif 

							/* Return no memory */
							pthread_exit((void*)ENOMEM);
						}

						/* Concating path and entry name to generate full path */
						sprintf(full_path, CONCAT_PATH_FRMT, path, entry->d_name);

						/* Printing full file path */
						printf(FILE_MATCH_SEARCH_FRMT, full_path);

						/* Free file full path */
						free(full_path);
					}					

					/* End of case */
					break;
				}

				/* In case it's a sub - directory */
				case (DT_DIR):
				{
					/* 4. If the file is a directory, don’t match its name to the search term. Instead, add that directory to the shared queue and wake up a thread to process it. */
					/* If it's a sub-directory, not current nor upper */
					if (!strcmp(".", entry->d_name) || !strcmp("..", entry->d_name))
					{
						/* Nothing to do */
					}
					else
					{
						/* Creating full path */
						full_path = (char*)calloc(strlen(path) + strlen(entry->d_name) + 2, sizeof(char));

						/* Validating allocation */
						if (!full_path)
						{
							#ifdef PRINT_BASIC_MESSAGES
								fprintf(stderr, FAILED_TO_ALLOCATE_MEMORY);
							#endif 

							/* Return no memory */
							pthread_exit((void*)ENOMEM);
						}

						/* Concating path and entry name to generate full path */
						sprintf(full_path, CONCAT_PATH_FRMT, path, entry->d_name);

						/* Locking queue mutex */
						pthread_mutex_lock(&s_queue_mutex_lock);

						/* Pushing directory to queue */
						if (!enqueue(&s_pQueueDirectories, full_path))
						{
							#ifdef PRINT_BASIC_MESSAGES
								fprintf(stderr, FAILED_TO_ALLOCATE_MEMORY);
							#endif 

							/* Free full path */
							free(full_path);

							/* Signaling queue is not empty */
							pthread_cond_signal(&s_queue_coinditional);

							/* Unlocking queue mutex */
							pthread_mutex_unlock(&s_queue_mutex_lock);	

							/* Return no memory */
							pthread_exit((void*)ENOMEM);
						}
						
						/* Signaling queue is not empty */
						pthread_cond_signal(&s_queue_coinditional);

						/* Unlocking queue mutex */
						pthread_mutex_unlock(&s_queue_mutex_lock);			
					}

					/* End of case */
					break;
				}

				/* Default case - other */
				default:
				{
					/* End of case */
					break;
				}
			}

			/* Test cancel thread */
			pthread_testcancel(); 
		}

		/* Closing directory */
		closedir(dir);

		/* Freeing current directory path */
		free(path);

		#ifdef PRINT_DEBUG_MESSAGES
			printf(THREAD_DONE_SEARCH_ITERATION_FRMT, lCurrentThreadIndex);
		#endif

		/* Test cancel thread */
		pthread_testcancel();

		/* 5. When done iterating through all files within the directory, repeat from 1. */
	}
}

/* 
 * Multithreaded search for search term in files names recursivly
 * 
 * Arguments:
 * 		strSearchRootDirectory 	– Search root directory.
 * 
 * Return value:
 * 		o All threads have exited due to an error: return code 1.
 * 		o There are no more directories in the queue and all current threads are done searching (idle). 
 * 		  If no thread has exited due to an error during the execution, return with code 0; 
 *  	  otherwise, return with code 1.
 */
int search(char *				strSearchRootDirectory)
{
	/* Variable Definition */
	void*     				pStatus			= NULL;
	char*					pRootDirectory	= NULL;
	int 					nTotalFound		= 0;
	int 					nThreadIndex 	= 0;
	int 					nReturnValue 	= EXIT_SUCCESS;

	/* Code Section */
	/* 1. Create a queue that holds directories. */
	s_pQueueDirectories = NULL;

	/* 2. Put the search root directory (where to start the search) in the queue. */
	pRootDirectory = strdup(strSearchRootDirectory);

	/* Validating string duplication */
	if (!pRootDirectory)
	{
		#ifdef PRINT_BASIC_MESSAGES
			fprintf(stderr, FAILED_TO_ALLOCATE_THREADS_ARRAY);
		#endif 

		/* Return failure */
		return EXIT_FAILURE;
	}

	/* Pushing root directory */
	enqueue(&s_pQueueDirectories, pRootDirectory);

	/* 3. Create n threads (the number received in argument 3). 
	 *    Each thread removes directories from the queue and searches 
	 *	  for file names with the specified term. 
	 */
	/* Allocating thread id's array */
	s_threads = (thread_with_status_t*)calloc(s_nNumberOfThreads, sizeof(thread_with_status_t));

	/* Validating allocation */
	if (!s_threads)
	{
		#ifdef PRINT_BASIC_MESSAGES
			fprintf(stderr, FAILED_TO_ALLOCATE_THREADS_ARRAY);
		#endif 

		/* Free duplicated string */
		free(pRootDirectory);

		/* Return failure */
		return EXIT_FAILURE;
	}

	/* Setting all threades states as created */
	for (nThreadIndex = 0; s_nNumberOfThreads > nThreadIndex; ++nThreadIndex)
	{
		s_threads[nThreadIndex].thread_state = THREAD_STATE_IDLE;
	}

	/* Setting SIGNIT handler to be able to cancel threads */
	if ((SIG_ERR == signal(SIGINT, cancel_theads_handler)))
	{
		/* Exit failure */
		return EXIT_FAILURE;
	}

	/* Creating threads and putting their id's in array */
	for (nThreadIndex = 0; s_nNumberOfThreads > nThreadIndex; ++nThreadIndex)
	{
		/* Creating thread */
		nReturnValue = pthread_create(&s_threads[nThreadIndex].thread_id, NULL, thread_search, (void *)nThreadIndex);

		/* Validatig thread creation */
        if (nReturnValue)
        {
			#ifdef PRINT_BASIC_MESSAGES
            	fprintf(stderr, FAILED_CREATING_THREAD_FRMT, strerror(nReturnValue));
			#endif

			/* Free duplicated string */
			free(pRootDirectory);

			/* Free threads array */
			free(s_threads);

            /* Return failure */
			return EXIT_FAILURE;
        }
	}

	/* Initializing match counter */
	nTotalFound = 0;

	/* 4. When there are no more directories in the queue, 
	 * 	  and all threads are idle (not searching for content within a directory) 
	 * 	  the program should exit with exit code 0, 
	 *    and print to stdout how many matching files were found. 
	 *    (printf(“Done searching, found %d files”, num_found)) 
	 */
	for (nThreadIndex = 0; s_nNumberOfThreads > nThreadIndex; ++nThreadIndex)
	{
		/* Waiting for thread to finish */
		nReturnValue = pthread_join(s_threads[nThreadIndex].thread_id, &pStatus);

		/* Adding result to match counter */
		nTotalFound += s_threads[nThreadIndex].result;

		/* Validatig thread creation */
        if (nReturnValue)
        {
			#ifdef PRINT_BASIC_MESSAGES
            	fprintf(stderr, THREAD_IS_ERRORNEOUS_FRMT, nThreadIndex, strerror(nReturnValue));
			#endif

            /* Return failure */
			nReturnValue = EXIT_FAILURE;
        }
	}

	/* Restoring to default signal handler for SIGINT */
	signal(SIGINT, SIG_DFL);

	/* Free threads array */
	free(s_threads);

	#ifdef PRINT_BASIC_MESSAGES
		printf(SEARCH_DONE_FRMT, nTotalFound);
	#endif
	
	/* Return value */
	return nReturnValue;
}

/* 
 * Init concurrency variables
 * 
 * Arguments: None
 * 
 * Exit value: EXIT_SUCCESS upon success
 */
int init_concurrency()
{
	/* Variable Definition */
	int 	nReturnValue = EXIT_SUCCESS;

	/* Code Section */	
	/* Initializing queue mutex */
	if (EXIT_SUCCESS != (nReturnValue = pthread_mutex_init(&s_queue_mutex_lock, NULL)))
	{
		/* Exit failure */
		return nReturnValue;
	}

	/* Initializing queue mutex */
	if (EXIT_SUCCESS != (nReturnValue = pthread_cond_init(&s_queue_coinditional, NULL)))
	{
		/* Exit failure */
		return nReturnValue;
	}

	/* Exit success */
	return EXIT_SUCCESS;
}

/* 
 * Destroy concurrency variables
 * 
 * Arguments: None
 * 
 * Exit value: EXIT_SUCCESS upon success
 */
int destroy_concurrency()
{
	/* Variable Definition */
	int nReturnValue = EXIT_SUCCESS;

	/* Code Section */	
	/* Destroying queue mutex */
	if (EXIT_SUCCESS != (nReturnValue = pthread_cond_destroy(&s_queue_coinditional)))
	{
		/* Exit failure */
		return nReturnValue;
	}

	/* Destroying queue mutex */
	if (EXIT_SUCCESS != (nReturnValue = pthread_mutex_destroy(&s_queue_mutex_lock)))
	{
		/* Exit failure */
		return nReturnValue;
	}	

	/* Return value */
	return nReturnValue;
}

/* 
 * Multithreaded search for search term in files names recursivly
 * 
 * Arguments:
 * 		argv[1] – Search root directory.
 * 		argv[2] – Search term.
 * 		argv[3] – Number of threads.
 * 
 * Exit value: 
 * 		o All threads have exited due to an error: exit the program with exit code 1.
 * 		o There are no more directories in the queue and all current threads are done searching (idle). 
 * 		  If no thread has exited due to an error during the execution, exit with exit code 0; 
 *  	  otherwise, exit with code 1.
 */
int main(int argc, char *argv[])
{
	/* Variable Definition */
	int 			nReturnValue 			= 0;	
	char* 			strSearchRootDirectory 	= NULL;

	/* Code Section */
	/* Validating arguments number */
	if (NUMBER_OF_ARGUMENTS != argc)
	{
		return EXIT_FAILURE;
	}

	/* Setting search root directory */
	strSearchRootDirectory = argv[SEARCH_ROOT_DIRECTORY];

	/* Setting search term */
	s_strSearchTerm = argv[SEARCH_TERM];

	/* Fetching number of threads */
	if ((1 != sscanf(argv[NUMBER_OF_THREADS], NUMBER_FORMAT, &s_nNumberOfThreads)) ||
		(0 >= s_nNumberOfThreads))
	{
		#ifdef PRINT_BASIC_MESSAGES
			fprintf(stderr, FAILED_TO_PARSE_NUMBER_OF_THREADS_FRMT, argv[NUMBER_OF_THREADS]);
		#endif

		/* Exit failure */
		exit(EXIT_FAILURE);
	}

	/* Initialize concurrency variables */
	if (EXIT_SUCCESS != init_concurrency())
	{
		#ifdef PRINT_BASIC_MESSAGES
			fprintf(stderr, FAILED_TO_INIT_CONCURRENCY);
		#endif

		/* Exit failure */
		exit(EXIT_FAILURE);
	}

	/* Multithreaded search of term as file name from root directory */
	nReturnValue = search(strSearchRootDirectory);
	
	/* Destory concurrency variables */	
	if (EXIT_SUCCESS != destroy_concurrency())
	{
		#ifdef PRINT_BASIC_MESSAGES
			fprintf(stderr, FAILED_TO_DESTROY_CONCURRENCY);
		#endif

		/* Exit failure */
		exit(EXIT_FAILURE);
	}

	/* Return value */
	pthread_exit((void*)nReturnValue);
}
