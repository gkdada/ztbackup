#ifndef CBACKUPFOLDER_H
#define CBACKUPFOLDER_H

#include "ExcludeList.h"

#define _FILE_OFFSET_BITS 64

#define MAJOR_VER       1
#define MINOR_VER       0

//ver. 0.1 - initial version
//     0.2 - smb support added
//     0.3 - ssh/sftp support added
//     0.4 - converted to gcc/configure/make
//     0.5 - added link no-recurse support
//     0.6 - removed ssh and libssh (in favor of ssh2 and libssh2)
//     0.7 - added .ztexclude support

#define DEFAULT_QUERY_TIMER_LENGTH    120000  //120 seconds

#define MIN_QUERY_TIMER_LENGTH        6000    //6 seconds

#define DEFAULT_SLEEP_LEN       20000   //20 milliseconds

#include <string>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "StringSink.h"
#include "LogFile.h"

//when a file exists in destination but not in source,
typedef enum fileOption
{
    fileOptionNotSet,
    fileOptionAsk,
    fileOptionLeave,
    fileOptionDelete
}FILE_OPTION;

typedef enum folderOption
{
    folderOptionNotSet,
    folderOptionAsk,
    folderOptionLeave,
    folderOptionDelete
}FOLDER_OPTION;



using namespace std;

//Initialize errors
typedef enum initStatus
{
    initNotDone = -1,
    initSuccess,
    initErrorNoCredentials,
    initErrorNoUsername,
    initErrorNoPassword,
    initErrorNoServername,
    initErrorInvalidPort,
    initErrorInvalidChar,
    initErrorResolvingServer,
    initErrorSocket,
    initErrorSession,
    initErrorAuthFailed,
    initErrorConnectFailed,
    initErrorSftpFailed,
    initErrorKeysFailed,
    initRootNotFound,
}INIT_STATUS;

//CopyFileToDest errors.
#define CFD_DEST_FULL           -1
#define CFD_SUCCESS             0
#define CFD_OPEN_FAIL           1
#define CFD_CREATE_FAIL         2
#define CFD_READ_FAIL           3
#define CFD_CREATEDIR_FAIL      4
#define CFD_WRITE_FAIL          5


typedef struct ZeroTouchStatistics
{
    ZeroTouchStatistics()
    {
        m_uiNumFolders = 0;
        m_uiNumFilesSkipped = 0;
        m_ullSizeFilesSkipped = 0;
        m_uiNumFilesCopied = 0;
        m_ullSizeFilesCopied = 0;
        m_uiNumFilesDeleted = 0;
        //m_ullSizeFilesDeleted = 0;
        m_uiNumFilesRestored = 0;
        m_ullSizeFilesRestored = 0;
    };
    uint32_t m_uiNumFolders;
    uint32_t m_uiNumFilesSkipped, m_uiNumFilesCopied, m_uiNumFilesDeleted, m_uiNumFilesRestored;
    uint64_t m_ullSizeFilesSkipped, m_ullSizeFilesCopied, /*m_ullSizeFilesDeleted, */m_ullSizeFilesRestored;
 
}ZT_STATISTICS,*LPZT_STATISTICS;

//all subsequent types of folder handling (for example, Local, Samba or SSH) will be derived from this class.

//this is a pure virtual class which can never be instantiated by itself.

class CBackupFolder
{
public:
    //this routine MUST NEVER be redefined in derived classes. ONLY redefine the pure virtual methods.
    int StartBackup(CBackupFolder* pDest, FILE_OPTION eFileOption, FOLDER_OPTION eFolderOption, bool bRecursive, CLogFile* pLogger);
    
    //this routine MUST NEVER be redefined. define InitializeToPath instead.
    //returns 0 for success.
    //pSrc should be Not NULL for destination folders. Requied to create folder with same access as source.
    INIT_STATUS Initialize(const char* szRootFolder, bool bAnonymousAccess, CBackupFolder* pSrc);
    
    //valid only for source folder.
    void printStats(uint32_t uiDestDeleteCount, CLogFile* pLogger);
    //the files are deleted from Destination folder. So, they need to be passed to source folder
    //for printing the statistics.
    uint32_t getDeletedCount(){return m_ztStats.m_uiNumFilesDeleted;};
private:
    //this routine MUST NEVER be redefined in derived classes. ONLY redefine the pure virtual methods.
    int BackupThis(const string& strRelativePath, CBackupFolder* pDest, FILE_OPTION eFileOption, FOLDER_OPTION eFolderOption, bool bRecursive);
    
    //this routine MUST NEVER be redefined in derived classes.
    //return 0 for success. negative CFD_xxx values to abort process. positive CFD_xxx values for non-fatal errors.
    int CopyFileToDest(const string& strRelativePath, CBackupFolder* pDest, bool bRestoring = false);
    
    //this is only useful to 'restore' a given folder, so bRestoring is true by default.
    //assumes that the folder doesn't exist in destination and we have to copy every single file or folder
    int CopyNewFolderToDest(const string& strRelativePath, CBackupFolder* pDest, bool bRestoring = true);
    
    //override if necessary. but shouldn't be.
    virtual const char* getRootPath(){return m_strRootPath.c_str();};
    
    //returns 'l' for "Leave", 'd' for "Delete", or 'r' for "Restore"
    //bIsAFile should be true to indicate a file and false to indicate a folder 
    char AskTimedDeleteQuestion(const char* szRelativePath, bool bIsAFile);
    //returns 'l' for "Leave", 'b' for "Backup", or 'r' for "Restore"
    char AskTimedRestoreQuestion(const char* szRelativePath, CBackupFolder* pDest);
    
    //returns whether a keypress is waiting.
    int KeyPressWaiting();

    //deletes the specified folder and ALL its contents. returns 0 for sucess.
    int deleteFolder(const string& strRelativePath);
    
protected:
    string m_strRootPath;   //derived classes are allowed to access this.
    ZT_STATISTICS m_ztStats;
    INIT_STATUS m_eInitStatus;
    friend class ExcludeList; //ExcludeList will access the m_strRootPath, openFile, closeFile and readFile
    
protected:
    CBackupFolder();
    virtual ~CBackupFolder();

    //returns current local time printed out in appropriate format into a string.
    string getTimeStr();
    //returns specified time printed out in appropriate format into a string.
    string getTimeStr(time_t tTimeVal);
    //returns 0 if initialization is successful. or one of the error codes in the BFI_xxx range.
    //pSrc should be Not NULL for destination folders. Requied to create folder with same access as source.
    virtual INIT_STATUS InitializeToPath(const char* szRootFolder, bool bAnonymousAccess, CBackupFolder* pSrc) = 0;
    //starts a search for files and folders. sym links are ignored for now. if it returns NULL search has ended and closed
    virtual const char* getFirst(const char* szRelativePath) = 0;
    //continues the search for files and folders. if it returns NULL, search has ended and closed.
    virtual const char* getNext(const char* szRelativePath) = 0;
    //returns true if the result of last getFirst/getNext is a file.
    virtual bool IsItAFile() = 0;
    //returns true if the result of last getFirst/getNext is a dir.
    virtual bool IsItADir() = 0;
    //returns true if the result of last getFirst/getNext is a symbolic link.
    virtual bool IsItALink() = 0;
    //this is under test. returns 1 if root folder of the entity is accessible. 0 if failed.
    virtual int ensureRootAccess() = 0;
    //returns the last modified time of file/directory. returns 0 if file/directory doesn't exist.
    virtual time_t getModTime(const char* szRelativePath) = 0;
    //returns the last accessed time of file/directory. returns 0 if file/directory doesn't exist.
    virtual time_t getAccTime(const char* szRelativePath) = 0;
    //sets the last modified time of a file/directory. returns 0 if successful. lastAccTime may not be used, depending on file system implementation.
    virtual int setModTime(const char* szRelativePath, time_t lastModTime, time_t lastAccTime) = 0;
    //returns file size. -1 if file/directory doesn't exist.
    virtual off64_t getFileSize(const char* szRelativePath) = 0;
    //opens the file for reading.
    virtual int openFile(const char* szRelativePath) = 0;
    //opens the file for writing (truncates to 0 if file exists)
    virtual int createFile(const char* szRelativePath) = 0;
    //creates a folder if it doesn't exist already
    virtual int createFolder(const char* szRelativePath, mode_t mode) = 0;
    //cloes the file opened for reading or writing.
    virtual int closeFile(int fHandle) = 0;
    //reads the specified number of bytes from file. returns number of bytes read.
    virtual off64_t readFile(int fHandle, char* dstBuffer, off64_t readCount) = 0;
    //writes the specified number of bytes to file. 0 for success. CFD_xxx for error.
    virtual int writeFile(int fHandle, const char* srcBuffer, off64_t writeCount) = 0;
    //deletes the specified file. returns 0 for success.
    virtual int deleteFile(const char* szRelativePath) = 0;
    //returns the mode of a file/directory.
    virtual mode_t getFileMode(const char* szRelativePath) = 0;
    //sets the mode of a file/directory. returns 0 if successful.
    virtual int setFileMode(const char* szRelativePath, mode_t mode) = 0;
    
    //deletes the specified file.
    virtual int unlinkFile(const char* szRelativePath) = 0;
    //removes the specified folder. The calling routing MUST ensure that the folder is empty.
    virtual int removeFolder(const char* szRelativePath) = 0;
    
    string addToPath(const string& strCurrent, const char* szAddend);
    string addToPath(const char* szCurrent, const char* szAddend);

public:
    friend class CLocalBackupFolder;
};

typedef CBackupFolder BACKUP_FOLDER,*LPBACKUP_FOLDER;

#endif // CBACKUPFOLDER_H
