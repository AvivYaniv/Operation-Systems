#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

/* Define Section */
#define PATH_SEPERATOR          "/"
#define DIRECTORY_ENV_NAME      "HW1DIR"
#define FILE_ENV_NAME           "HW1TF"

#define PRINT_REPLACED_FRMT     "%s"

#define OLD_STRING_ARG_INDEX     1
#define NEW_STRING_ARG_INDEX     2

/* Methods Section */
char* getEnvironmentVariable(char* strName)
{
    /* Variable Section */
    char* strVariable;

    /* Code Section */
    /* Getting environment variable */
    strVariable = getenv(strName);

    /* If no variable exit failure */
    if (NULL == strVariable)
    {
        /* Exit Failure */
        exit(EXIT_FAILURE);
    }

    /* Return environment variable */
    return strVariable;
}

char* buildFilePath(char* strDirectory, char* strFileName)
{
    /* Variable Definition */
    char* strFilePath;

    /* Code Section */
    /* Allocating the file path */
    strFilePath = (char *)malloc(
        strlen(strDirectory) + 
        strlen(PATH_SEPERATOR) + 
        strlen(strFileName) + 
        1);

    /* Validating allocation */
    if (NULL == strFilePath)
    {
        /* Exit Failure */
        exit(EXIT_FAILURE);
    }

    /* Building the file path */
    strcpy(strFilePath, strDirectory);
    strcat(strFilePath, PATH_SEPERATOR);
    strcat(strFilePath, strFileName);

    /* Returning file path */
    return strFilePath;
}

/* Read file and return it's contetnt as string */
char* readFile(char* strFilePath)
{
    /* Variable Definition */
    struct stat fsFileStat;
    ssize_t stBytesRead;
    ssize_t stTotalBytesRead;
    char* strFileContent;
    int fdFileDescriptor;
    long lFileSize;
    long lContentSize;

    /* Code Section */
    /* Opening file */
    fdFileDescriptor = open(strFilePath, O_RDONLY);

    /* Validating file openning succedded */
    if (0 > fdFileDescriptor) {
        /* Free the memory of file path allocation */
        free(strFilePath);

        /* Exit Failure */
        exit(EXIT_FAILURE);
    }

    /* Get file status */    
    if (0 > stat(strFilePath, &fsFileStat))
    {
        /* Free the memory of file path allocation */
        free(strFilePath);

        /* Close file */
        close(fdFileDescriptor);

        /* Exit Filure */
        exit(EXIT_FAILURE);
    }

    /* Fetching file size */
    lFileSize = fsFileStat.st_size;
    lContentSize = lFileSize + 1L;

    /* Allocating memory for file */
    strFileContent = (char *)malloc(lContentSize);

    /* Validating allocation */    
    if (NULL == strFileContent)
    {
        /* Free the memory of file path allocation */
        free(strFilePath);

        /* Close file */
        close(fdFileDescriptor);

        /* Exit failure */
        exit(EXIT_FAILURE);
    }
    
    /* Read file */
    stBytesRead = 0;
    stTotalBytesRead = 0;

    /* While there are bytes to read */
    while (stTotalBytesRead < lFileSize)
    {
        stBytesRead = read(fdFileDescriptor, strFileContent + stTotalBytesRead, lFileSize);
        stTotalBytesRead += stBytesRead;
        
        /* Checking bytes has been read */
        if ((0 > stBytesRead) || (lFileSize < stTotalBytesRead))
        {
            /* Free the memory of file path allocation */
            free(strFilePath);

            /* Free the memory of file content */
            free(strFileContent);

            /* Exit failure */
            exit(EXIT_FAILURE);
        }
    }

    /* Setting zero-terminator to set as string */
    strFileContent[lContentSize-1]='\0';

    /* Returning file size */
    return strFileContent;
}

/* Counting number of needles in haystack */
int countOccurences(char* strHaystack, const char* strNeedle)
{
    /* Variable Definition */
    int nOccurences;
    int nNeedleLength;
    char* pCurrent;
    char* pFirstOccurence;

    /* Code Section */
    nNeedleLength = strlen(strNeedle);

    /* Iterating on the haystack, and counting occurences */
    pCurrent = strHaystack;
    for (nOccurences = 0; (pFirstOccurence = strstr(pCurrent, strNeedle)); ++nOccurences) {
        pCurrent = pFirstOccurence + nNeedleLength;
    }

    /* Return number of occurences */
    return nOccurences;
}

/* Replace all occurences of old with new in original, and return replaced */
char* replaceString(char* strOriginal, char* strOld, char* strNew) {
    /* Variable Definition */
    char* strReplaced;
    char* pCurrentReplaced;
    char* pCurrentOriginal;
    char* pNextOld;
    int nOldLength;
    int nNewLength;
    int nOccurences;
    int nReplacedLength;
    int nRegularToCopy;
    int nCopyIterations;

    /* Code Section */
    /* Getting length of old & new */
    nOldLength = strlen(strOld);    
    nNewLength = strlen(strNew);

    /* Iterating over the text and  */
    nOccurences = countOccurences(strOriginal, strOld);
    
    /* Calculating replaced length, based on diffrence between replacments */
    nReplacedLength = 
        strlen(strOriginal) + 
        (nOccurences * (nNewLength - nOldLength)) + 
        1;

    /* Allocating memory for the replaced string */
    strReplaced = (char*)malloc(nReplacedLength);

    /* If allocation error for replaced */
    if (NULL == strReplaced)
    {
        /* Free original */
        free(strOriginal);

        /* Exit failure */
        exit(EXIT_FAILURE);
    }

    /* Copying original and replacing old by new */
    pCurrentReplaced = strReplaced;
    pCurrentOriginal = strOriginal;
    for(nCopyIterations = nOccurences; 0 < nCopyIterations; --nCopyIterations)
    {
        /* Finding the next occurence of string to replace */
        pNextOld = strstr(pCurrentOriginal, strOld);

        /* Copying all chars till that next occurence and adjusting pointers */
        nRegularToCopy = pNextOld - pCurrentOriginal;
        pCurrentReplaced = strncpy(pCurrentReplaced, pCurrentOriginal, nRegularToCopy);
        pCurrentReplaced += nRegularToCopy;
        pCurrentOriginal += nRegularToCopy;

        /* Copying new string chars and adjusting pointers */
        pCurrentReplaced = strcpy(pCurrentReplaced, strNew);
        pCurrentReplaced += nNewLength;
        pCurrentOriginal += nOldLength;
    }

    /* Copying the rest of the original string */
    strcpy(pCurrentReplaced, pCurrentOriginal);

    /* Setting zero-terminator to set as string */
    strReplaced[nReplacedLength-1] = '\0';

    /* Returning replaced string */
    return strReplaced;
}

/* Main Function */
/* Print content of file, where every occurence of old is replaced by new */
int main(int argc, char *argv[])
{
    /* Variable Section */
    char* strOld;
    char* strNew;
    char* strFilePath;
    char* strDirectory=NULL;
    char* strFileName=NULL;
    char* strFileOriginalContent;
    char* strFileReplacedContent;

    /* Code Section*/
    // Setting old and new strings to replace
    strOld = argv[OLD_STRING_ARG_INDEX];
    strNew = argv[NEW_STRING_ARG_INDEX];

    /* Getting directory environment variable, if non-exsistant exit error*/
    strDirectory = getEnvironmentVariable(DIRECTORY_ENV_NAME);

    /* Getting file environment variable, if non-exsistant exit error*/
    strFileName = getEnvironmentVariable(FILE_ENV_NAME);

    /* Building file path */
    strFilePath = buildFilePath(strDirectory, strFileName);

    /* Read file content */
    strFileOriginalContent = readFile(strFilePath);

    /* Free the memory of file path allocation */
    free(strFilePath);

    /* Replace in content old with new */
    strFileReplacedContent = 
        replaceString(strFileOriginalContent, strOld, strNew);

    /* Free original content */
    free(strFileOriginalContent);

    /* Printing replaced content */
    printf(PRINT_REPLACED_FRMT, strFileReplacedContent);

    /* Free replaced content */
    free(strFileReplacedContent);

    /* Return Success */
    return EXIT_SUCCESS;
}
