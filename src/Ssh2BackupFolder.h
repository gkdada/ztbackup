#ifndef CSSH2BACKUPFOLDER_H
#define CSSH2BACKUPFOLDER_H

#include "libssh2.h"
#include "libssh2_sftp.h"
#include "BackupFolder.h" // Base class: CBackupFolder

#define MAX_NUM_SFTP_FILES  20

//SSH Authentication Methods (SAM)
#define SAM_PASSWORD    1   //authenticate with password.
#define SAM_KBD         2   //KBD interactive.
#define SAM_PK_AUTO     4   //Public key auto

#define DIR_BUF_MAXLEN      8192
//#define DIR_LONGENT_MAXLEN  4096

#define USE_AGENT_CODE

class CSsh2BackupFolder : public CBackupFolder
{
public:
    CSsh2BackupFolder();
    virtual ~CSsh2BackupFolder();

public:
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
    //returns 0 for success. 1 if some portion is missing.
    INIT_STATUS splitSshPath(const char* szRootFolder);
    int saveHandle(LIBSSH2_SFTP_HANDLE* fHandle);
    INIT_STATUS AuthenticateWithPublicKey();

    string m_strUsername, m_strPassword, m_strServerLocation;//if password is specified in path.
    int m_iPort;    //defaults to 22.
    string m_strSshRootPath;   //"onefolder/twofolder" part of the path.
    int m_sSocket;
    
    LIBSSH2_SESSION* m_pSshSession;
    LIBSSH2_SFTP* m_pSftpSession;
    LIBSSH2_SFTP_HANDLE* m_SftpDir;
    LIBSSH2_SFTP_ATTRIBUTES m_lastFoundEntry;
#ifdef USE_AGENT_CODE
    LIBSSH2_AGENT* m_pSshAgent;
#endif
    string m_strLastFoundPath;
    char m_DirBuffer[DIR_BUF_MAXLEN];
    //char m_szLongEntry[DIR_LONGENT_MAXLEN];
    
    
    //we need to store SFTP file handles, because SFTP file handles are structs rather than ints.
    LIBSSH2_SFTP_HANDLE* m_fileList[MAX_NUM_SFTP_FILES];
};

#endif // CSSH2BACKUPFOLDER_H
