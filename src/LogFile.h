#ifndef CLOGFILE_H
#define CLOGFILE_H

#include <string>

using namespace std;

class CLogFile
{
public:
    CLogFile(const char* szLogPath);
    ~CLogFile();
    
    size_t AddToLog(const char* szString);
    
protected:
    int m_fHandle;
    string m_strLogFilePath;
};



#endif //CLOGFILE_H