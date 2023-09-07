#include "LocalBackupFolder.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <utime.h>
#include <string.h>

CLocalBackupFolder::CLocalBackupFolder()
{
    m_dirSearch = NULL;
}

CLocalBackupFolder::~CLocalBackupFolder()
{
    if(m_dirSearch)
    {
        closedir(m_dirSearch);
        m_dirSearch = NULL;
    }
}

INIT_STATUS CLocalBackupFolder::InitializeToPath(const char* szRootFolder, bool bAnonymousAccess, CBackupFolder* pSrc)
{
    //we just need to make sure the folder exists. No special process necessary to 
    time_t tRoot = getModTime("");
    if(tRoot == 0)
    {
        if(pSrc != NULL)    //this means this is a destination folder
        {
            int iResult = createFolder("", pSrc->getFileMode(""));
            if(iResult)
                return initRootNotFound;
        }
        return initRootNotFound;//folder doesn't exist.
    }
    return initSuccess;
}
bool CLocalBackupFolder::IsItADir()
{
    if(m_lastFoundEntry != NULL)
    {
        if(m_lastFoundEntry->d_type == DT_DIR)
            return true;
    }
    return false;
/*    struct stat bufStat;
    //it is a conscious decision to use lstat instead of stat.
    //this is because - at this point - we want to avoid handling symbolic links.
    //we just skip them and process only regular files and folders.
    int iResult = lstat(m_strLastFoundPath.c_str(),&bufStat);
    if(iResult)
        return false;
    if(S_ISDIR(bufStat.st_mode))
        return true;
    return false;*/
}
bool CLocalBackupFolder::IsItAFile()
{
    if(m_lastFoundEntry != NULL)
    {
        if(m_lastFoundEntry->d_type == DT_REG)
            return true;
    }
    return false;
/*
    struct stat bufStat;
    //it is a conscious decision to use lstat instead of stat.
    //this is because - at this point - we want to avoid handling symbolic links.
    //we just skip them and process only regular files and folders.
    int iResult = lstat(m_strLastFoundPath.c_str(),&bufStat);
    if(iResult)
        return false;
    if(S_ISREG(bufStat.st_mode))
        return true;
    return false;*/
}

bool CLocalBackupFolder::IsItALink()
{
    if(m_lastFoundEntry != NULL)
    {
        if(m_lastFoundEntry->d_type == DT_LNK)
            return true;
    }
    return false;
}

int CLocalBackupFolder::closeFile(int fHandle)
{
    int iResult = close(fHandle);
    return iResult;
}
int CLocalBackupFolder::createFile(const char* szRelativePath)
{
    string strPath = addToPath(m_strRootPath,szRelativePath);
    //create a file or if the file exists, truncate it.
    int fHandle = creat(strPath.c_str(), S_IRUSR|S_IWUSR);  //this mode will be changed to original file's mode after closing.
    //no further processing necessary.
    return fHandle;
}
int CLocalBackupFolder::createFolder(const char* szRelativePath, mode_t mode)
{
    string strPath = addToPath(m_strRootPath,szRelativePath);
    struct stat bufStat;
    int iResult = lstat(strPath.c_str(),&bufStat);
    if(iResult == 0)    //dir/file exists.
    {
        if(S_ISDIR(bufStat.st_mode))
            return 0;   //It exists and is a directory.
        printf("A file/link by the name %s already exists. Cannot create directory.\r\n", strPath.c_str());
        return CFD_CREATEDIR_FAIL;
    }
    //so it doesn't exist.
    if(errno == EEXIST)//race condition?
        return 0;   //hope for the best. At worst, subsequent file operations will fail.
    iResult = mkdir(strPath.c_str(),S_IRUSR|S_IWUSR);  //we don't actually need to worry about upstream paths since they will all have to exist before we reach here.
    if(iResult == 0 || errno == EEXIST)
    {
        chmod(strPath.c_str(),mode);
        return 0;
    }
    if(errno == ENAMETOOLONG)
    {
        printf("Cannot create directory %s. Path name is too long.\r\n",strPath.c_str());
    }
    else
        printf("Error creating directory %s.\r\n", strPath.c_str());
    return CFD_CREATEDIR_FAIL;
    
    
}
int CLocalBackupFolder::deleteFile(const char* szRelativePath)
{
    m_ztStats.m_uiNumFilesDeleted++;
    string strPath = addToPath(m_strRootPath,szRelativePath);
    int iResult = unlink(strPath.c_str());
    return iResult;
}
/*
int CLocalBackupFolder::deleteFolder(const string& strRelativePath)
{
    string strPath = addToPath(m_strRootPath,strRelativePath.c_str());
    //we need to delete ALL files & all sub-folders.
    CStringSink dirList;
    const char* fName = getFirst(strRelativePath.c_str());
    while(fName != NULL)
    {
        if(IsItADir())
        {
            if(strcmp(fName,".") && strcmp(fName,".."))
                dirList.addString(fName);
        }
        else //otherwise, just delete it. we don't care if it is a symbolic link or anything.
        {
            m_ztStats.m_uiNumFilesDeleted++;
            string strTemp = addToPath(m_strRootPath,strRelativePath.c_str());
            strTemp += "/";
            strTemp += fName;
            unlink(strTemp.c_str());
        }
        fName = getNext(strRelativePath.c_str());
    }
    CStringSink* pCur = &dirList;
    while(pCur)
    {
        const char* pTemp = pCur->getString().c_str();
        if(pTemp[0] == 0)    //no more directories.
            break;
        string strTemp = addToPath(strRelativePath,pTemp);
        deleteFolder(strTemp);
        pCur = pCur->getNext();
    }
    
    //finally remove the folder.
    int iResult = rmdir(strPath.c_str());
    if(iResult)
        printf("Error %u removing folder %s.\r\n", errno, strPath.c_str());
    return iResult;
}*/

int CLocalBackupFolder::unlinkFile(const char* szRelativePath)
{
    string strTemp = addToPath(m_strRootPath,szRelativePath);
    int iResult = unlink(strTemp.c_str());
    return iResult;
}

int CLocalBackupFolder::removeFolder(const char* szRelativePath)
{
    string strTemp = addToPath(m_strRootPath, szRelativePath);
    int iResult = rmdir(strTemp.c_str());
    return iResult;
}


mode_t CLocalBackupFolder::getFileMode(const char* szRelativePath)
{
    string strPath = addToPath(m_strRootPath, szRelativePath);
        
    struct stat bufStat;
    int iResult = lstat(strPath.c_str(),&bufStat);
    if(iResult == 0)    //dir/file exists.
        return bufStat.st_mode;
    return 0;
    
}
off64_t CLocalBackupFolder::getFileSize(const char* szRelativePath)
{
    string strPath = addToPath(m_strRootPath, szRelativePath);
        
    struct stat bufStat;
    int iResult = lstat(strPath.c_str(),&bufStat);
    if(iResult == 0)    //dir/file exists.
    {
        if(S_ISDIR(bufStat.st_mode))//directories don't have size!
            return 0;
        return bufStat.st_size;
    }
    return 0;
}

int CLocalBackupFolder::ensureRootAccess()
{
    string strPath = addToPath(m_strRootPath,"");
    DIR* pFolder = opendir(strPath.c_str());
    if(pFolder == NULL)
        return 0;
    //folder exists. that's enough
    closedir(pFolder);
    return 1;
}

const char* CLocalBackupFolder::getFirst(const char* szRelativePath)
{
    m_strLastFoundPath.clear();
    m_lastFoundEntry = NULL;
    if(m_dirSearch)
    {
        closedir(m_dirSearch);
        m_dirSearch = NULL;
    }
    
    string strPath = addToPath(m_strRootPath, szRelativePath);
    
    m_dirSearch = opendir(strPath.c_str());
    if(!m_dirSearch)
        return NULL;
    return getNext(szRelativePath);
}
const char* CLocalBackupFolder::getNext(const char* szRelativePath)
{
    m_strLastFoundPath.clear();
    if(m_dirSearch == NULL)
        return NULL;
    m_lastFoundEntry = readdir(m_dirSearch);
    if(m_lastFoundEntry == NULL)
    {
        closedir(m_dirSearch);
        m_dirSearch = NULL;
        return NULL;
    }
    m_strLastFoundPath = addToPath(m_strRootPath,szRelativePath);
    m_strLastFoundPath += "/";
    m_strLastFoundPath += m_lastFoundEntry->d_name;
    return m_lastFoundEntry->d_name;
}

time_t CLocalBackupFolder::getModTime(const char* szRelativePath)
{
    string strPath = addToPath(m_strRootPath, szRelativePath);
        
    struct stat bufStat;
    int iResult = lstat(strPath.c_str(),&bufStat);
    if(iResult == 0)    //dir/file exists.
        return bufStat.st_mtime;
    return 0;
}

time_t CLocalBackupFolder::getAccTime(const char* szRelativePath)
{
    string strPath = addToPath(m_strRootPath, szRelativePath);
        
    struct stat bufStat;
    int iResult = lstat(strPath.c_str(),&bufStat);
    if(iResult == 0)    //dir/file exists.
        return bufStat.st_atime;
    return 0;
}

const char* CLocalBackupFolder::getRootPath()
{
    return m_strRootPath.c_str();
}
int CLocalBackupFolder::openFile(const char* szRelativePath)
{
    string strPath = addToPath(m_strRootPath,szRelativePath);
    int fHandle = open(strPath.c_str(), O_RDONLY);  
    //no further processing necessary.
    return fHandle;
}
off64_t CLocalBackupFolder::readFile(int fHandle, char* dstBuffer, off64_t readCount)
{
    ssize_t iRead = read(fHandle, dstBuffer, readCount);
    //no processing necessary.
    return iRead;
}
int CLocalBackupFolder::setFileMode(const char* szRelativePath, mode_t mode)
{
    //keep only file mode bits.
    mode &= (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);

    string strPath = addToPath(m_strRootPath, szRelativePath);
   int iResult = chmod(strPath.c_str(), mode);
   return iResult;
}
int CLocalBackupFolder::setModTime(const char* szRelativePath, time_t lastModTime, time_t lastAccTime)
{
    struct utimbuf times;
    times.actime = lastAccTime;
    times.modtime = lastModTime;
    
    string strPath = addToPath(m_strRootPath, szRelativePath);
    
    int iResult = utime(strPath.c_str(), &times);
    return iResult;
    
}
int CLocalBackupFolder::writeFile(int fHandle, const char* srcBuffer, off64_t writeCount)
{
    ssize_t iWrite = write(fHandle, srcBuffer, writeCount);
    if(iWrite == -1)
    {
        if(errno == ENOSPC)
        {
            return CFD_DEST_FULL;
        }

    }
    if(iWrite != writeCount)
        return CFD_WRITE_FAIL;
    return 0;
}
