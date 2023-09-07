#include "SmbBackupFolder.h"
#include <string.h>
#include <glib.h>
#include <errno.h>

bool g_bUseAnonymousAccess = false;

static void
sbf_get_auth_data_with_context_fn(SMBCCTX * context,
                              const char * pServer,
                              const char * pShare,
                              char * pWorkgroup,
                              int maxLenWorkgroup,
                              char * pUsername,
                              int maxLenUsername,
                              char * pPassword,
                              int maxLenPassword);

static void
sbf_get_auth_data_fn(const char * pServer,
                 const char * pShare,
                 char * pWorkgroup,
                 int maxLenWorkgroup,
                 char * pUsername,
                 int maxLenUsername,
                 char * pPassword,
                 int maxLenPassword);

CSmbBackupFolder::CSmbBackupFolder()
{
    m_Sctx = smbc_new_context();
    m_iDirHandle = 0;
    m_eSmbInitStatus = initNotDone;
}

CSmbBackupFolder::~CSmbBackupFolder()
{
    if(m_iDirHandle)
    {
        smbc_closedir(m_iDirHandle);
        m_iDirHandle = 0;
    }
    smbc_free_context(m_Sctx,1);
}

INIT_STATUS CSmbBackupFolder::InitializeToPath(const char* szRootFolder, bool bAnonymousAccess, CBackupFolder* pSrc)
{
    //1. we need to find a way to return the error in finding the username/password in sbf_get_auth_data_fn
    //     back to initialization routine.
    //2. we need to fix the sbf_get_auth_data_fn to find username/password for server/share first and (if
    //     that is not available) find username/password for server - without share.
    //      IN OTHER WORDS, try the more specific option first and general option later.
                
                
    smbc_setFunctionAuthData(m_Sctx, sbf_get_auth_data_fn);
    smbc_setOptionUseKerberos(m_Sctx, 1);
    smbc_setOptionFallbackAfterKerberos(m_Sctx, 1);

    if(!smbc_init_context(m_Sctx))
    {
        smbc_free_context(m_Sctx, 0);
        printf("Error initializing context.\r\n");
        if(m_eSmbInitStatus != initNotDone)
            return m_eSmbInitStatus;
        return initRootNotFound;  //initialization error.
    }
    smbc_set_context(m_Sctx);
    return initSuccess;   //we don't actually open the directory here. we do it in getFirst.
                //accordingly an error message is displayed THERE when we fail to open the root folder.
}
bool CSmbBackupFolder::IsItADir()
{
    if(m_lastFoundEntry != NULL)
    {
        if(m_lastFoundEntry->smbc_type == SMBC_DIR)
            return true;
    }
    return false;
}
bool CSmbBackupFolder::IsItAFile()
{
    if(m_lastFoundEntry != NULL)
    {
        if(m_lastFoundEntry->smbc_type == SMBC_FILE)
            return true;
    }
    return false;
}
int CSmbBackupFolder::closeFile(int fHandle)
{
    int iResult = smbc_close(fHandle);
    return iResult;
}
int CSmbBackupFolder::createFile(const char* szRelativePath)
{
    string strPath = addToPath(m_strRootPath,szRelativePath);
    //create a file or if the file exists, truncate it.
    int fHandle = smbc_creat(strPath.c_str(), S_IRUSR|S_IWUSR);  //this mode will be changed to original file's mode after closing.
    //no further processing necessary.
    return fHandle;
}
int CSmbBackupFolder::createFolder(const char* szRelativePath, mode_t mode)
{
    string strPath = addToPath(m_strRootPath,szRelativePath);
    struct stat bufStat;
    int iResult = smbc_stat(strPath.c_str(),&bufStat);
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
    iResult = smbc_mkdir(strPath.c_str(),S_IRUSR|S_IWUSR);  //we don't actually need to worry about upstream paths since they will all have to exist before we reach here.
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
int CSmbBackupFolder::deleteFile(const char* szRelativePath)
{
    m_ztStats.m_uiNumFilesDeleted++;
    string strPath = addToPath(m_strRootPath,szRelativePath);
    int iResult = smbc_unlink(strPath.c_str());
    return iResult;
}

/*
int CSmbBackupFolder::deleteFolder(const string& strRelativePath)
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
            smbc_unlink(strTemp.c_str());
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
    int iResult = smbc_rmdir(strPath.c_str());
    if(iResult)
        printf("Error %u removing folder %s.\r\n", errno, strPath.c_str());
    return iResult;
}*/

int CSmbBackupFolder::unlinkFile(const char* szRelativePath)
{
    string strTemp = addToPath(m_strRootPath,szRelativePath);
    int iResult = smbc_unlink(strTemp.c_str());
    return iResult;
}

int CSmbBackupFolder::removeFolder(const char* szRelativePath)
{
    string strTemp = addToPath(m_strRootPath, szRelativePath);
    int iResult = smbc_rmdir(strTemp.c_str());
    return iResult;
}

time_t CSmbBackupFolder::getAccTime(const char* szRelativePath)
{
    string strPath = addToPath(m_strRootPath, szRelativePath);
        
    struct stat bufStat;
    int iResult = smbc_stat(strPath.c_str(),&bufStat);
    if(iResult == 0)    //dir/file exists.
        return bufStat.st_atime;
    return 0;
}
mode_t CSmbBackupFolder::getFileMode(const char* szRelativePath)
{
    string strPath = addToPath(m_strRootPath, szRelativePath);
        
    struct stat bufStat;
    int iResult = smbc_stat(strPath.c_str(),&bufStat);
    if(iResult == 0)    //dir/file exists.
        return bufStat.st_mode;
    return 0;
}
off64_t CSmbBackupFolder::getFileSize(const char* szRelativePath)
{
    string strPath = addToPath(m_strRootPath, szRelativePath);
        
    struct stat bufStat;
    int iResult = smbc_stat(strPath.c_str(),&bufStat);
    if(iResult == 0)    //dir/file exists.
    {
        if(S_ISDIR(bufStat.st_mode))//directories don't have size!
            return 0;
        return bufStat.st_size;
    }
    return 0;
}

int CSmbBackupFolder::ensureRootAccess()
{
    string strPath = addToPath(m_strRootPath, "");
    int iHandle = smbc_opendir(strPath.c_str());
    if(iHandle < 0)
        return 0;
    smbc_closedir(iHandle);
    return 1;
}
const char* CSmbBackupFolder::getFirst(const char* szRelativePath)
{
    if(m_iDirHandle)
    {
        smbc_closedir(m_iDirHandle);
        m_iDirHandle = 0;
    }
    m_strLastFoundPath.clear();
    m_lastFoundEntry = NULL;

    string strPath = addToPath(m_strRootPath, szRelativePath);
    
    m_iDirHandle = smbc_opendir(strPath.c_str());
    if(m_iDirHandle < 0)
    {
        m_iDirHandle = 0;
        if(szRelativePath[0] == 0)  //this is the source/dest folder. we need to print an error message here
        {                           //since (unlike CLocalBackupFolder), we don't open the folder in 
                                    //InitializeToPath (and hence Initialize() wouldn't have caught the error.
            printf("Error opening source folder %s.\r\n",strPath.c_str());
        }
        return NULL;
    }
    
    return getNext(szRelativePath);
    
}
time_t CSmbBackupFolder::getModTime(const char* szRelativePath)
{
    string strPath = addToPath(m_strRootPath, szRelativePath);
        
    struct stat bufStat;
    int iResult = smbc_stat(strPath.c_str(),&bufStat);
    if(iResult == 0)    //dir/file exists.
        return bufStat.st_mtime;
    return 0;
}
const char* CSmbBackupFolder::getNext(const char* szRelativePath)
{
    m_strLastFoundPath.clear();
    if(m_iDirHandle == 0)
        return NULL;
    m_lastFoundEntry = smbc_readdir(m_iDirHandle);
    if(m_lastFoundEntry == NULL)
    {
        smbc_closedir(m_iDirHandle);
        m_iDirHandle = 0;
        return NULL;
    }
    m_strLastFoundPath = addToPath(m_strRootPath,szRelativePath);
    m_strLastFoundPath += "/";
    m_strLastFoundPath += m_lastFoundEntry->name;
    return m_lastFoundEntry->name;
}
const char* CSmbBackupFolder::getRootPath()
{
    return m_strRootPath.c_str();
}
int CSmbBackupFolder::openFile(const char* szRelativePath)
{
    string strPath = addToPath(m_strRootPath,szRelativePath);
    int fHandle = smbc_open(strPath.c_str(), O_RDONLY, S_IRUSR|S_IWUSR);  
    //no further processing necessary.
    return fHandle;
}
off64_t CSmbBackupFolder::readFile(int fHandle, char* dstBuffer, off64_t readCount)
{
    ssize_t iRead = smbc_read(fHandle, dstBuffer, readCount);
    //no processing necessary.
    return iRead;
}
int CSmbBackupFolder::setFileMode(const char* szRelativePath, mode_t mode)
{
    //keep only file mode bits.
    mode &= (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);

    string strPath = addToPath(m_strRootPath, szRelativePath);
   int iResult = smbc_chmod(strPath.c_str(), mode);
   return iResult;
}
int CSmbBackupFolder::setModTime(const char* szRelativePath, time_t lastModTime, time_t lastAccTime)
{
    struct timeval times[2];
    times[0].tv_sec = lastAccTime;
    times[0].tv_usec = 0;
    times[1].tv_sec = lastModTime;
    times[1].tv_usec = 0;
    
    string strPath = addToPath(m_strRootPath, szRelativePath);
    
    int iResult = smbc_utimes(strPath.c_str(), times);
    return iResult;
}
int CSmbBackupFolder::writeFile(int fHandle, const char* srcBuffer, off64_t writeCount)
{
    ssize_t iWrite = smbc_write(fHandle, srcBuffer, writeCount);
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

static void sbf_get_auth_data_fn(const char * pServer,
                 const char * pShare,
                 char * pWorkgroup,
                 int maxLenWorkgroup,
                 char * pUsername,
                 int maxLenUsername,
                 char * pPassword,
                 int maxLenPassword)
{
    char            server[256] = { '\0' };
    char            share[256] = { '\0' };
    char            workgroup[256] = { '\0' };
    char            username[256] = { '\0' };
    char            password[256] = { '\0' };

    static int krb5_set = 1;
    
    
    if (strcmp(server, pServer) == 0 &&
        strcmp(share, pShare) == 0 &&
        *workgroup != '\0' &&
        *username != '\0')
    {
        strncpy(pWorkgroup, workgroup, maxLenWorkgroup - 1);
        strncpy(pUsername, username, maxLenUsername - 1);
        strncpy(pPassword, password, maxLenPassword - 1);
        return;
    }
    
    if(g_bUseAnonymousAccess)
    {
        pWorkgroup[0] = 0;
        pUsername[0] = 0;
        pPassword[0] = 0;
        return;
    }

    string pConfFile;
    char* pHomeDir = getenv("HOME");
     if(pHomeDir == NULL)
    {
        pConfFile = "~";
    }
    else
        pConfFile = pHomeDir;
    pConfFile += "/.smbcreds";
    GKeyFile* pFile = g_key_file_new();
    GError* pError = NULL;
    
    char SectionName[1000];
    sprintf(SectionName,"%s/%s",pServer,pShare);
 
    bool bLoaded = g_key_file_load_from_file(pFile,pConfFile.c_str(),G_KEY_FILE_NONE,&pError);
    if(bLoaded != false)
    {
        //load from credentials.
        //first look for Server/Share section.
        //if not found, search for Server section.
        gchar* pName = g_key_file_get_string(pFile,SectionName,"username",&pError);
        if(pName == NULL)
        {
            strncpy(SectionName, pServer, maxLenUsername - 1);
            pName = g_key_file_get_string(pFile,SectionName,"username",&pError);
        }
        gchar* pPass = g_key_file_get_string(pFile,SectionName,"password",&pError);
        gchar* pDomain = g_key_file_get_string(pFile,SectionName,"workgroup",&pError);
        if(pDomain == NULL)
            pDomain = g_key_file_get_string(pFile,SectionName,"domain",&pError);
            
        if(pName != NULL && pPass != NULL)  //it's a go!
        {
            if(pDomain != NULL)
            {
                strncpy(pWorkgroup, pDomain, maxLenWorkgroup - 1);
                strncpy(workgroup, pDomain, sizeof(workgroup) - 1);
            }
            strncpy(pUsername, pName, maxLenUsername - 1);
            strncpy(username, pName, sizeof(username) - 1);
            strncpy(pPassword, pPass, maxLenPassword - 1);
            strncpy(password, pPass, sizeof(password) - 1);
            
            //also save the share.
            strncpy(server,pServer,sizeof(server) - 1);
            strncpy(share, pShare, sizeof(share) - 1);

            krb5_set = 1;
            return;
        }
            
    }

    if (krb5_set && getenv("KRB5CCNAME")) {
      krb5_set = 0;
      return;
    }


}

static void
sbf_get_auth_data_with_context_fn(SMBCCTX * context,
                              const char * pServer,
                              const char * pShare,
                              char * pWorkgroup,
                              int maxLenWorkgroup,
                              char * pUsername,
                              int maxLenUsername,
                              char * pPassword,
                              int maxLenPassword)
{
    printf("Authenticating with context %p", context);
    if (context != NULL) {
        char *user_data = (char*)smbc_getOptionUserData(context);
        printf(" with user data %s", user_data);
    }
    printf("\n");

    sbf_get_auth_data_fn(pServer, pShare, pWorkgroup, maxLenWorkgroup,
                     pUsername, maxLenUsername, pPassword, maxLenPassword);
}
