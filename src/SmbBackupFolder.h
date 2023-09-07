#ifndef CSMBBACKUPFOLDER_H
#define CSMBBACKUPFOLDER_H

#include <samba-4.0/libsmbclient.h>

#include "BackupFolder.h" // Base class: CBackupFolder

class CSmbBackupFolder : public CBackupFolder
{
public:
    CSmbBackupFolder();
    virtual ~CSmbBackupFolder();

protected:
    virtual INIT_STATUS InitializeToPath(const char* szRootFolder, bool bAnonymousAccess, CBackupFolder* pSrc);
    virtual bool IsItADir();
    virtual bool IsItAFile();
    //as of now, links are checked for local folders only
    virtual bool IsItALink(){return false;}
    virtual int ensureRootAccess();
    virtual int closeFile(int fHandle);
    virtual int createFile(const char* szRelativePath);
    virtual int createFolder(const char* szRelativePath, mode_t mode);
    virtual int deleteFile(const char* szRelativePath);
    //virtual int deleteFolder(const string& strRelativePath);
    virtual time_t getAccTime(const char* szRelativePath);
    virtual mode_t getFileMode(const char* szRelativePath);
    virtual off64_t getFileSize(const char* szRelativePath);
    virtual const char* getFirst(const char* szRelativePath);
    virtual time_t getModTime(const char* szRelativePath);
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
    int m_iDirHandle;   //used for opening and enumerating a directory.
    smbc_dirent* m_lastFoundEntry;
    string m_strLastFoundPath;
    SMBCCTX* m_Sctx;
    INIT_STATUS m_eSmbInitStatus;   //set by samba specific init routines and returned by InitializeToPath as necessary.
};

#endif // CSMBBACKUPFOLDER_H
