#include "LogFile.h"
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

CLogFile::CLogFile(const char* szLogPath)
{
    m_fHandle = -1;
    
    char szLogFilename[100];
    
    time_t tCurrent;
    time(&tCurrent);
    struct tm* tmLocal = localtime(&tCurrent);
    
    sprintf(szLogFilename, "%04u%02u.log",tmLocal->tm_year+1900,tmLocal->tm_mon+1);
    
    m_strLogFilePath = szLogPath;
    m_strLogFilePath += "/";
    m_strLogFilePath += szLogFilename;
    
    m_fHandle = open(m_strLogFilePath.c_str(), O_WRONLY | O_APPEND | O_CREAT, S_IRUSR|S_IWUSR);
}

CLogFile::~CLogFile()
{
    if(m_fHandle != -1)
    {
        close(m_fHandle);
        m_fHandle = -1;
    }
}

size_t CLogFile::AddToLog(const char* szString)
{
    if(m_fHandle != -1)
    {
        int iLen = strlen(szString);
        if(iLen == write(m_fHandle, szString, iLen))
            return 0;
    }
    return -1;
}
