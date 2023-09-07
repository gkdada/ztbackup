#pragma once

#include <list>
//#include "BackupFolder.h"
#include "Util.h"

class CBackupFolder;

typedef std::list<std::string> ExcludePatterns;

class ExcludeList
{
public:
    ExcludeList(CBackupFolder* pParent, const std::string& strRelativePath);
    ~ExcludeList();
    
    bool isExcluded(const char* szName);
    
    static void PrintDebugMessages(){s_bDebugMessages =true;};

private:
    static bool s_bDebugMessages;
    
    CBackupFolder* m_pParent;

    void processBuffer(char* caBuffer, off64_t iBufSize, off64_t& iLeftoverCount, bool bEndOfFile);
    void addToList(const char* caBuffer, off64_t iLength);
    
    
    ExcludePatterns m_PatternList;
};