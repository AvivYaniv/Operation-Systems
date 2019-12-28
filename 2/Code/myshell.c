#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

/**
 * @about 	Homework 2 - Operation Systems
 * Shell with ability to run foreground, background and pipe
 * @author 	Aviv Yaniv  
 */

/* Define Section */
#define BACKGROUND_PROCESSES_DELIMETER	"&"
#define NAMELESS_PIPES_DELIMETER		"|"

/* Message Section */
#define ERROR_FORKING_MSG				"Error forking: "
#define ERROR_EXECUTING_MSG				"Error executing: "
#define ERROR_CREATING_PIPE_MSG			"Error creating pipe: "
#define ERROR_DUP_FILE_DESC_MSG			"Error duplicatinf file descriptor: "
#define ERROR_CHANGE_HANDLER_MSG		"Error changing handler: "

/* Enums Section */
typedef enum BOOL
{ 
	FALSE 	=	0, 
	TRUE	=	1,
} BOOL;

typedef enum eAuxillaryReturnValue {
	AUXILARRY_RETURN_SUCCESS	=	0,
	AUXILARRY_RETURN_ERROR		=	1,
} eAuxillaryReturnValue;

typedef enum eProccessReturnValue {
	PROCESS_RETURN_ERROR		=	0,
	PROCESS_RETURN_SUCCESS		=	1,
} eProccessReturnValue;

typedef enum eArglistType {
	REGULAR_ARGLIST,
	BACKGROUND_PROCESS_ARGLIST,
	NAMELESS_PIPE_ARGLIST,
} eArglistType;

/* Static Definition */
static pid_t s_pidShellPID 	= 	0;

/* Methods Section */
void always_ignore_signal_handler(int sig) 
{
	/* Ignore and return */
    return;
}

void shell_ignore_signal_handler(int sig) 
{
	/* Code Section */
	/* If not shell - handle signal */
	if (getpid() != s_pidShellPID)
	{
		/* Execute default signale handler */
		signal(sig, SIG_DFL);
		raise(SIGINT);
	}
}

/**
 * @brief  	After prepare() finishes, the parent (shell) should not terminate upon a SIGINT.
 * @retval 	Return
 * 				AUXILARRY_RETURN_SUCCESS	-	Return upon successfull handler change
 * 				AUXILARRY_RETURN_ERROR		-	Return upon erroneous handler change
 */
int prepare(void)
{
	/* Variable Section */
	void* tHandler;

	/* Code Section */
	/* Setting shell PID in a static varaible to distinguish it from foreground childrens */
	s_pidShellPID = getppid();

	/* Setting shell process to ignore SIGNIT signal */
	tHandler = signal(SIGINT, shell_ignore_signal_handler);

	/* Return value */
	return (SIG_ERR != tHandler) ? AUXILARRY_RETURN_SUCCESS : AUXILARRY_RETURN_ERROR;
}

/**
 * @brief  	Detects the type of the arglist given
 * 
 * @param  	count: The number of arguments in the arglist (last is null)
 * @param  	arglist: The arglist to classify
 * @param  	arglistType: Out parameter of the arglist detected
 * @retval 	An index of delimeter:
 * 				BACKGROUND_PROCESSES_DELIMETER or
 * 				NAMELESS_PIPE_ARGLIST or
 * 				negative number of none of the above
 */
static int arglistTypeDetector(int count, char** arglist, eArglistType* pArglistType)
{
	/* Variable Definition */
	int 	nArgIndex;
	char* 	strArg;
	int 	nDelimeterIndex;
	
	/* Code Section */
	/* Presuming it's a regular type arglist */
	nDelimeterIndex = -1;
	*pArglistType = REGULAR_ARGLIST;

	/* Going over the arglist, searching for delimeters */
	for (nArgIndex = 0; nArgIndex < count; ++nArgIndex)
	{
		/* Fetching current argument */
		strArg = arglist[nArgIndex];

		/* If argument is nonexistant */
		if (!strArg)
		{
			/* Should never get here upon guidelines correctness */
			break;
		}
		/* Else if, current argument is backgroud process delimeter */
		else if (!strcmp(BACKGROUND_PROCESSES_DELIMETER, strArg))
		{
			nDelimeterIndex = nArgIndex;
			*pArglistType = BACKGROUND_PROCESS_ARGLIST;
			break;
		}
		/* Else if, current argument is nameless pipe delimeter */
		else if (!strcmp(NAMELESS_PIPES_DELIMETER, strArg))
		{
			nDelimeterIndex = nArgIndex;
			*pArglistType = NAMELESS_PIPE_ARGLIST;
			break;
		}
	}

	/* Return value */
	return nDelimeterIndex;
}

static void nullifyArgument(char** arglist, int nIndexToNullify)
{
	/* Code Section */
	/* Nullify argument at specific index */
	arglist[nIndexToNullify] = NULL;
}

/**
 * @brief  	Regular type: 	Shall run the child process on foreground
 *							Shall wait for child to finish
 *							Foreground child processes should terminate upon SIGINT.
 * 
 * @param  	arglist: The argument list to execute
 * @retval 	Return value of father process (child exits)
 * 				PROCESS_RETURN_SUCCESS	-	Return success
 * 				PROCESS_RETURN_ERROR	-	Return error
 */
static eProccessReturnValue regularArglistExecutor(char** 	arglist)
{
	/* Variable Definition */
	pid_t 	pid;
	int 	status;

	/* Code Section */
	/* Forking to create child proccess */
	pid = fork();

	/* If child process */
	if (!pid) 
	{
		/* If arglist command fails to execute */
		if (0 > execvp(arglist[0], arglist)) 
		{
			/* Printing error executing message */
			perror(ERROR_EXECUTING_MSG);

			/* Exitting child with failure */
			exit(EXIT_FAILURE);
		}
	} 
	/* Else if, error forking */
	else if (0 > pid) 
	{
		/* Print error forking */
		perror(ERROR_FORKING_MSG);

		/* Return error */
		return PROCESS_RETURN_ERROR;
	} 
	/* Else, parent process */
	else 
	{
		/* Wait for foreground child to finish (prevents it from becoming zombie) */
		do 
		{
			waitpid(pid, &status, WUNTRACED);
		} 
		while (!WIFEXITED(status) && !WIFSIGNALED(status));
	}

	/* Return success */
	return PROCESS_RETURN_SUCCESS;
}

/**
 * @brief  	Background process:	Shall run the child process on background
 *								Shall NOT wait for child to finish
 *								Background child processes should not terminate upon SIGINT.
 * 
 * @param  	arglist: The argument list to execute
 * @retval 	Return value of father process (child exits)
 * 				PROCESS_RETURN_SUCCESS	-	Return success
 * 				PROCESS_RETURN_ERROR	-	Return error
 */
static eProccessReturnValue backgroundProcessArglistExecutor(char** 	arglist, 
							 								 int 	nDelimeterIndex)
{
	/* Variable Definition */
	pid_t 					pid;
	eProccessReturnValue	eReturnValue;

	/* Code Section */
	/* Setting as default, will change upon error */
	eReturnValue = PROCESS_RETURN_SUCCESS;

	/* Setting parent process to ignore SIGNIT signal -
	   so forked background child will always ignore the SIGNIT signal */
	if (SIG_ERR == signal(SIGINT, always_ignore_signal_handler))
	{
		/* Printing error changing handler message */
		perror(ERROR_CHANGE_HANDLER_MSG);

		/* Return error */
		return PROCESS_RETURN_ERROR;
	}

	/* Preventing zombie child */
	if (SIG_ERR == signal(SIGCHLD, SIG_IGN))
	{
		/* Printing error changing handler message */
		perror(ERROR_CHANGE_HANDLER_MSG);

		/* Setting return error */
		return PROCESS_RETURN_ERROR;
	}

	/* Forking to create child proccess */
	pid = fork();

	/* If child process */
	if (!pid) 
	{
		/* If arglist command fails to execute */
		if (0 > execvp(arglist[0], arglist)) 
		{
			/* Printing error executing message */
			perror(ERROR_EXECUTING_MSG);

			/* Exitting child with failure */
			exit(EXIT_FAILURE);
		}
	} 
	/* Else if, error forking */
	else if (0 > pid) 
	{
		/* Print error forking */
		perror(ERROR_FORKING_MSG);

		/* Return error */
		return PROCESS_RETURN_ERROR;
	} 
	/* Else, parent process */
	else 
	{
		/* Setting shell process to ignore SIGNIT signal */
		if (SIG_ERR == signal(SIGINT, shell_ignore_signal_handler))
		{			
			/* Printing error changing handler message */
			perror(ERROR_CHANGE_HANDLER_MSG);

			/* Setting return error */
			eReturnValue = PROCESS_RETURN_ERROR;
		}
	}

	/* Return success */
	return eReturnValue;
}

/**
 * @brief  	Nameless pipe:		Shall run two child processes on foreground
 *								Shall wait for children to finish
 *								Foreground child processes should terminate upon SIGINT.
 * 
 * @param  	arglist: The argument list to execute
 * @retval 	Return value of father process (child exits)
 * 				PROCESS_RETURN_SUCCESS	-	Return success
 * 				PROCESS_RETURN_ERROR	-	Return error
 */
static eProccessReturnValue namelessPipeArglistExecutor(char** 	arglist, 
							 							int 	nDelimeterIndex)
{
	/* Variable Definition */
    pid_t 					p1; 
	pid_t 					p2; 
	int 					status;
    int 					pipefd[2];  /* 0 is read end, 1 is write end */
	BOOL					bIsErrorInChild;
	
	char** 					argFirstProcess 	= arglist;
	char** 					argSecondProcess	= arglist + nDelimeterIndex + 1;
  
	/* Code Section */
	/* Setting as default, will change upon error */
	bIsErrorInChild = FALSE;

	/* Creting pipe */
    if (0 > pipe(pipefd)) 
	{ 
		/* Print error creating pipe */
		perror(ERROR_CREATING_PIPE_MSG);

		/* Return error */
		return PROCESS_RETURN_ERROR;
    } 

	/* Forking to create first child proccess */
    p1 = fork(); 

	/* If first child */
    if (!p1) 
	{ 
		/* Setting as default, will change upon error */
		bIsErrorInChild = FALSE;

        /* Preparing the write end */
        close(pipefd[0]); 
		bIsErrorInChild = (0 > dup2(pipefd[1], STDOUT_FILENO)) ? TRUE : FALSE;
        close(pipefd[1]);

		/* If error occured while duplicating file descriptor */
		if (bIsErrorInChild)
		{
			/* Printing error duplicating file descriptor message */
			perror(ERROR_DUP_FILE_DESC_MSG);

			/* Exitting child with failure */
			exit(EXIT_FAILURE);
		}

		/* If arglist command fails to execute */
        if (0 > execvp(arglist[0], argFirstProcess)) 
		{ 
			/* Printing error executing message */
			perror(ERROR_EXECUTING_MSG);

			/* Exitting child with failure */
			exit(EXIT_FAILURE);
        }
    } 
	/* Else if, forking has failed */
	else if (0 > p1)
	{ 
		/* Print error forking */
		perror(ERROR_FORKING_MSG);

		/* Return error */
		return PROCESS_RETURN_ERROR;
    }
	/* Else, parent proccess */
	else 
	{ 
		/* Wait for foreground child to finish */
		do 
		{
			waitpid(p1, &status, WUNTRACED);
		} 
		while (!WIFEXITED(status) && !WIFSIGNALED(status));

        /* Forking to create second child proccess */ 
        p2 = fork(); 
  
        /* If second child */
        if (!p2) 
		{ 
			/* Setting as default, will change upon error */
			bIsErrorInChild = FALSE;

			/* Preparing the read end */
            close(pipefd[1]); 
			bIsErrorInChild = (0 > dup2(pipefd[0], STDIN_FILENO)) ? TRUE : FALSE;
            close(pipefd[0]);

			/* If error occured while duplicating file descriptor */
			if (bIsErrorInChild)
			{
				/* Printing error duplicating file descriptor message */
				perror(ERROR_DUP_FILE_DESC_MSG);

				/* Exitting child with failure */
				exit(EXIT_FAILURE);
			}

			/* If arglist command fails to execute */
            if (0 > execvp(arglist[nDelimeterIndex + 1], argSecondProcess)) 
			{ 
                /* Printing error executing message */
				perror(ERROR_EXECUTING_MSG);

				/* Exitting child with failure */
				exit(EXIT_FAILURE);
            } 
        } 
		/* Else if, forking has failed */
		else if (0 > p2) 
		{ 
			/* Print error forking */
			perror(ERROR_FORKING_MSG);

			/* Return error */
			return PROCESS_RETURN_ERROR;
        } 
		/* Else, parent proccess */
		else 
		{
            /* Closing pipe */
			close(pipefd[0]);
			close(pipefd[1]); 			

			/* Wait for foreground child to finish (prevents it from becoming zombie) */
			do 
			{
				waitpid(p2, &status, WUNTRACED);
			} 
			while (!WIFEXITED(status) && !WIFSIGNALED(status));
        } 
    } 

	/* Return success */
	return PROCESS_RETURN_SUCCESS;
}

/**
 * @brief  	Handling arglist according to it's type
 * 			Regular type: 		Shall run the child process on foreground
 * 								Shall wait for child to finish
 * 								Foreground child processes should terminate upon SIGINT.
 * 			Background process:	Shall run the child process on background
 * 								Shall NOT wait for child to finish
 * 								Background child processes should not terminate upon SIGINT.
 *			Nameless pipe:		Shall run two child processes on foreground
 * 								Shall wait for children to finish
 * 								Foreground child processes should terminate upon SIGINT.
 *
 * @param  	count: 		Number of arguments in list
 * @param  	arglist: 	The argument list, last one is NULL
 * @retval 	According to eProccessReturnValue:
 *				PROCESS_RETURN_ERROR		=	0,
 *				PROCESS_RETURN_SUCCESS		=	1,
 */
int process_arglist(int count, char** arglist)
{
	/* Enum Definition */
	eArglistType 			tArglistType;
	eProccessReturnValue	eReturnValue;

	/* Variable Definition */
	int 					nDelimeterIndex;

	/* Code Section */
	/* Setting return value to default value, may be changed to error */
	eReturnValue = PROCESS_RETURN_SUCCESS;

	/* Detecting arglist type */
	nDelimeterIndex = arglistTypeDetector(count, arglist, &tArglistType);

	/* Handling arguments according to arglist type */
	switch (tArglistType)
	{
		/* In case it's a regular arglist */
		case (REGULAR_ARGLIST):
		{
			/* Execute regular arglist */
			eReturnValue = regularArglistExecutor(arglist);

			/* End of case */
			break;
		}
		
		/* In case it's a background process arglist */
		case (BACKGROUND_PROCESS_ARGLIST):
		{
			/* Nullify delimeter */
			nullifyArgument(arglist, nDelimeterIndex);

			/* Execute background process arglist */
			eReturnValue = backgroundProcessArglistExecutor(arglist, nDelimeterIndex);

			/* End of case */
			break;
		}

		/* In case it's a nameless pipe arglist */
		case (NAMELESS_PIPE_ARGLIST):
		{
			/* Nullify delimeter */
			nullifyArgument(arglist, nDelimeterIndex);

			/* Execute nameless pipe arglist */
			eReturnValue = namelessPipeArglistExecutor(arglist, nDelimeterIndex);

			/* End of case */
			break;
		}
	}

	/* Return value */
	return eReturnValue;
}

/**
 * @retval Return success 0
 */
int finalize(void)
{
	/* Return value */
	return AUXILARRY_RETURN_SUCCESS;
}
