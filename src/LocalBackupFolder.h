#ifndef CLOCALBACKUPFOLDER_H
#define CLOCALBACKUPFOLDER_H

#include <sys/types.h>
#include <dirent.h>

#include "BackupFolder.h" // Base class: CBackupFolder

class CLocalBackupFolder : public CBackupFolder
{
public:
    CLocalBackupFolder();
    virtual ~CLocalBackupFolder();

protected:
    virtual INIT_STATUS InitializeToPath(const char* szRootFolder, bool bAnonymousAccess, CBackupFolder* pSrc);
    virtual bool IsItADir();
    virtual bool IsItAFile();
    virtual bool IsItALink();
    virtual int ensureRootAccess();
    virtual int closeFile(int fHandle);
    virtual int createFile(const char* szRelativePath);
    virtual int createFolder(const char* szRelativePath, mode_t mode);
    virtual int deleteFile(const char* szRelativePath);
    //virtual int deleteFolder(const string& strRelativePath);
    virtual mode_t getFileMode(const char* szRelativePath);
    virtual off64_t getFileSize(const char* szRelativePath);
    virtual const char* getFirst(const char* szRelativePath);
    virtual time_t getModTime(const char* szRelativePath);
    virtual time_t getAccTime(const char* szRelativePath);
    virtual const char* getNext(const char* szRelativePath);
    virtual const char* getRootPath();
    virtual int openFile(const char* szRelativePath);
    virtual off64_t readFile(int fHandle, char* dstBuffer, off64_t readCount);
    virtual int setFileMode(const char* szRelativePath, mode_t mode);
    virtual int setModTime(const char* szRelativePath, time_t lastModTime, time_t lastAccTime);
    virtual int writeFile(int fHandle, const char* srcBuffer, off64_t writeCount);
    virtual int unlinkFile(const char* szRelativePath);
    virtual int removeFolder(const char* szRelativePath);
    
private:
    DIR* m_dirSearch;
    dirent* m_lastFoundEntry;
    string m_strLastFoundPath;//full path of the last found entry.
    
};

#endif // CLOCALBACKUPFOLDER_H
