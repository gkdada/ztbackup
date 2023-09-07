#include "Ssh2BackupFolder.h"

#include <sys/socket.h>
#include <netdb.h>

CSsh2BackupFolder::CSsh2BackupFolder()
{
    m_iPort = 22;
    m_pSshSession = NULL;
    m_pSftpSession = NULL;
    m_SftpDir = NULL;
    for(int i=0;i<MAX_NUM_SFTP_FILES;i++)
        m_fileList[i] = NULL;
    m_sSocket = -1;
#ifdef USE_AGENT_CODE
    m_pSshAgent = NULL;
#endif
}

CSsh2BackupFolder::~CSsh2BackupFolder()
{
#ifdef USE_AGENT_CODE
    if(m_pSshAgent != NULL)
        libssh2_agent_free(m_pSshAgent);
#endif
    for(int i=0;i<MAX_NUM_SFTP_FILES;i++)
    {
        if(m_fileList[i] != NULL)
            libssh2_sftp_close_handle(m_fileList[i]);
    }
    
    if(m_SftpDir)
        libssh2_sftp_close_handle(m_SftpDir);
    if(m_pSftpSession)
    {
        libssh2_sftp_shutdown(m_pSftpSession);
    }
    if(m_pSshSession)
    {
        libssh2_session_disconnect(m_pSshSession,"Closing SFTP session.");
        libssh2_session_free(m_pSshSession);
    }
    if(m_sSocket != -1)
        close(m_sSocket);
}

bool IsSplitDelimiterChar(char cCurrent)
{
    //special characters: ':', '@', and '/'. Also zero termination.
    if(cCurrent == ':' || cCurrent == '@' || cCurrent == '/' || cCurrent == 0)
        return true;
    return false;
}

INIT_STATUS CSsh2BackupFolder::splitSshPath(const char* szRootFolder)
{
    m_strUsername.clear();
    m_strPassword.clear();
    m_strServerLocation.clear();
    m_strSshRootPath.clear();
    
    //ssh://username[:password]@serveriporname[:port]/onefolder/twofolder......
    int iCur = 0;
    if(!strncmp(szRootFolder,"ssh://",6))//MUST be. Otherwise, we wouldn't even come here!
        iCur = 6;
    //start copying username.
    while(!IsSplitDelimiterChar(szRootFolder[iCur]))
        m_strUsername += szRootFolder[iCur++];
    if(szRootFolder[iCur] == '/' || szRootFolder[iCur] == 0)
    {
        printf("SSH path specified is missing the username portion.\r\n");
        return initErrorNoUsername;
    }
    if(szRootFolder[iCur] == ':')//there is a password.
    {
        iCur++;
        while(!IsSplitDelimiterChar(szRootFolder[iCur]))
            m_strPassword += szRootFolder[iCur++];
        if(szRootFolder[iCur] != '@')
        {
            printf("SSH path specified is missing '@servername' portion.\r\n");
            return initErrorNoServername;
        }
    }
    iCur++;//after '@'
    while(!IsSplitDelimiterChar(szRootFolder[iCur]))
        m_strServerLocation += szRootFolder[iCur++];
    if(szRootFolder[iCur] == ':')
    {
        //there is a port number.
        iCur++;
        m_iPort = atoi(&szRootFolder[iCur]);
        if(m_iPort == 0 || m_iPort > 65534)
        {
            printf("Invalid port number specified in SSH path. Must be between 0 and 65534.\r\n");
            return initErrorInvalidPort;
        }
        while(!IsSplitDelimiterChar(szRootFolder[iCur]))
            iCur++; //skip over the port number.
    }
    //now, what follows is the path.
    if(szRootFolder[iCur] == '/')
    {
        iCur++;
        while(szRootFolder[iCur])
            m_strSshRootPath += szRootFolder[iCur++];
    }
    else if(szRootFolder[iCur] != 0)//nothing else should be here after servername [and port number].
    {
        printf("Invalid special character '%c' after server name/ip  (and port number, if any).\r\n",szRootFolder[iCur]);
        return initErrorInvalidChar;
    }
    
    //TODO: decide whether it is prudent or not.
    m_strRootPath = m_strSshRootPath;
    
    return initSuccess;
}

INIT_STATUS CSsh2BackupFolder::InitializeToPath(const char* szRootFolder, bool bAnonymousAccess, CBackupFolder* pSrc)
{
    //we need to breakdown the path into its components. For example:
    INIT_STATUS eResult = splitSshPath(szRootFolder);
    if(eResult)
        return eResult;
    
    struct hostent* pEnt = gethostbyname(m_strServerLocation.c_str());
    if(pEnt == NULL)
    {
        printf("Error resolving the ssh server %s\r\n",m_strServerLocation.c_str());
        return initErrorResolvingServer;
    }
    
    m_sSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(m_sSocket == -1)
    {
        printf("Error creating a socket. Aborting SSH connection attempt.\r\n");
        return initErrorSocket;
    }
    
    struct in_addr** addr_list = (struct in_addr **) pEnt->h_addr_list;
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(m_iPort);
    sin.sin_addr.s_addr = addr_list[0]->s_addr;
    
    if(connect(m_sSocket, (sockaddr*)&sin, sizeof(sockaddr_in)) != 0)
    {
        printf("Error connecting a socket. Aborting SSH connection attempt.\r\n");
        return initErrorSocket;
    }
       
        
    m_pSshSession = libssh2_session_init();
    
    if(m_pSshSession == NULL)
    {
        printf("Error creating an ssh session.\r\n");
        return initErrorSession;
    }
    
    libssh2_session_set_blocking(m_pSshSession,1);
    
    int rc = libssh2_session_handshake(m_pSshSession, m_sSocket);
    if(rc)
    {
        printf("Failed to establish SSH session. Aborting...\r\n");
        return initErrorSession;
    }
    
    //TODO: get the fingerprint and compare it to known hosts.
    char* userauthlist = libssh2_userauth_list(m_pSshSession, m_strUsername.c_str(), m_strUsername.length());
    int auth_pw = 0;
    printf("Authentication methods: %s\n", userauthlist);
    if (strstr(userauthlist, "password") != NULL) {
        auth_pw |= SAM_PASSWORD;
    }
    if (strstr(userauthlist, "keyboard-interactive") != NULL) {
        auth_pw |= SAM_KBD;
    }
    if (strstr(userauthlist, "publickey") != NULL) {
        auth_pw |= SAM_PK_AUTO;
    }
    
    if(m_strPassword.length())
    {
        //we NEED to authenticate with password.
        if(auth_pw & SAM_PASSWORD)
        {
            rc = libssh2_userauth_password(m_pSshSession, m_strUsername.c_str(), m_strPassword.c_str());
            if(rc)
            {
                printf("Authentication with password failed. Aborting SSH Connection attempt.\r\n");
                return initErrorAuthFailed;
            }
        }
        else if(auth_pw & SAM_PK_AUTO)//try public key.
        {
#ifdef USE_AGENT_CODE
            //if(AuthenticateWithPublicKey())
            //    return initErrorAuthFailed;
#else
            printf("Username: '%s'\r\n",m_strUsername.c_str());
           rc = libssh2_userauth_publickey_fromfile(m_pSshSession, m_strUsername.c_str(), g_keyfile1,
                                                   g_keyfile2, "");
            if(rc) {
                printf("\tAuthentication by public key failed!\n");
                char *errmsg = new char[1000];
                int iErrMsgLen = 999;
                int iError = libssh2_session_last_error(m_pSshSession, &errmsg, &iErrMsgLen, 1);
                errmsg[iErrMsgLen] = 0;
                printf("libssh2_session_last_error returned code %d, string %s\r\n", iError, errmsg);
                free(errmsg);
                return initErrorAuthFailed;
            }
#endif
        }
        else
        {
            printf("Unknown authentication method. ZtB is only able to handle password and ssh-agent authentication.\r\n");
            return initErrorAuthFailed;
        }
    }
    else if(auth_pw & SAM_PK_AUTO)
    {
#ifdef USE_AGENT_CODE
        if(AuthenticateWithPublicKey())
            return initErrorAuthFailed;
#else
            printf("Username: '%s'\r\n",m_strUsername.c_str());
           rc = libssh2_userauth_publickey_fromfile(m_pSshSession, m_strUsername.c_str(), g_keyfile1,
                                                   g_keyfile2, 0);
            if(rc) {
                printf("\tAuthentication by public key failed!\n");
                char *errmsg = new char[1000];
                int iErrMsgLen = 999;
                int iError = libssh2_session_last_error(m_pSshSession, &errmsg, &iErrMsgLen, 1);
                errmsg[iErrMsgLen] = 0;
                printf("libssh2_session_last_error returned code %d, string %s\r\n", iError, errmsg);
                free(errmsg);
                return initErrorAuthFailed;
            }
#endif
    }
    else
    {
        printf("Unknown authentication method. ZtB is only able to handle password and ssh-agent authentication.\r\n");
        return initErrorAuthFailed;
    }
    
    m_pSftpSession = libssh2_sftp_init(m_pSshSession);
    if(m_pSftpSession == NULL)
    {
        printf("Error initializing SFTP session. Aborting...\r\n");
        return initErrorAuthFailed;
    }
    
    
#if 0
    int verbosity = SSH_LOG_FUNCTIONS;
    
    ssh_options_set(m_SshSession, SSH_OPTIONS_HOST, m_strServerLocation.c_str()); 
    ssh_options_set(m_SshSession, SSH_OPTIONS_PORT, &m_iPort);
    ssh_options_set(m_SshSession,SSH_OPTIONS_USER, m_strUsername.c_str());
    ssh_options_set(m_SshSession, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);    
    
    int rc = ssh_connect(m_SshSession);
    if(rc != SSH_OK)
    {
        printf("Error connecting to SSH server %s: %s\n",m_strServerLocation.c_str(),ssh_get_error(m_SshSession));        
        return initErrorConnectFailed;
    }
    
    if(m_strPassword.length())
        rc = ssh_userauth_password(m_SshSession, NULL, m_strPassword.c_str());
    else
        rc = ssh_userauth_publickey_auto(m_SshSession, NULL, NULL);
    if(rc != SSH_AUTH_SUCCESS)
    {
        printf("Error %s authenticating with SSH server %s.\r\n",ssh_get_error(m_SshSession), m_strServerLocation.c_str());
        return initErrorAuthFailed;
    }
    
    //connect sftp.
    m_SftpSession = sftp_session(m_SshSession);
    if (m_SftpSession == NULL)
    {
        printf("Error allocating SFTP session for SSH Server %s: %s\n", m_strServerLocation.c_str(), ssh_get_error(m_SshSession));
        return initErrorSftpFailed;
    }

    rc = sftp_init(m_SftpSession);
    if (rc != SSH_OK)
    {
        printf("Error %d initializing SFTP session for SSH server %s: %s (%u) (%u).\n",rc, m_strServerLocation.c_str(),ssh_get_error(m_SshSession),ssh_get_error_code(m_SshSession),sftp_get_error(m_SftpSession));
        return initErrorSftpFailed;
    }
#endif
    //we don't open the folder here since the SFTP session is up.
    //we open the folder (and print the error, if any) in getFirst().
    return initSuccess;
}
//#if 0
INIT_STATUS CSsh2BackupFolder::AuthenticateWithPublicKey()
{
    struct libssh2_agent_publickey *identity, *prev_identity = NULL;
    
    m_pSshAgent = libssh2_agent_init(m_pSshSession);
    if(m_pSshAgent == NULL)
    {
        printf("Error connecting to SSH Agent for keys. Try running ssh-add before running ZtB.\r\n");
        return initErrorKeysFailed;
    }
    int rc = libssh2_agent_connect(m_pSshAgent);
    if(rc)
    {
        printf("Error connecting to SSH Agent.Try running ssh-add before running ZtB.\r\n");
        return initErrorConnectFailed;
    }
    rc = libssh2_agent_list_identities(m_pSshAgent);
    if(rc)
    {
        printf("Error requesting identities from SSH Agent. Try running ssh-add before running ZtB.\r\n");
        return initErrorKeysFailed;
    }
    while(1)
    {
        rc = libssh2_agent_get_identity(m_pSshAgent,&identity, prev_identity);
        if(rc == 1)
        {
            printf("breaking because libssh2_agent_get_identity returned 1\r\n");
            break;
        }
        if(rc < 0)
        {
            printf("Error obtaining identity from SSH Agent. Try running ssh-add before running ZtB.\r\n");
            return initErrorKeysFailed;
        }
        else
        {
            printf("We found a public key with identity %s.\r\n", identity->comment);
        }
        rc = libssh2_agent_userauth(m_pSshAgent, m_strUsername.c_str(), identity);
        if(rc == 0)
            return initSuccess;   //user authentication succeeded.
        else
        {
            printf("libssh2_agent_userauth for '%s' returned %d\r\n", m_strUsername.c_str(), rc);
            char *errmsg = new char[1000];
            int iErrMsgLen = 999;
            int iError = libssh2_session_last_error(m_pSshSession, &errmsg, &iErrMsgLen, 1);
            errmsg[iErrMsgLen] = 0;
            printf("libssh2_session_last_error returned code %d, string %s\r\n", iError, errmsg);
            free(errmsg);
        }
        prev_identity = identity;
    }

    printf("Couldn't continue authentication. Try running ssh-add before running ZtB.\r\n");
    return initErrorConnectFailed;
}

//#endif

bool CSsh2BackupFolder::IsItADir()
{
    if(m_lastFoundEntry.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)
    {
       if(m_lastFoundEntry.permissions & LIBSSH2_SFTP_S_IFDIR)
           return true;
    }
    return false;
}
bool CSsh2BackupFolder::IsItAFile()
{
    if(m_lastFoundEntry.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)
    {
        if(m_lastFoundEntry.permissions & LIBSSH2_SFTP_S_IFREG)
            return true;
    }
    return false;
}
int CSsh2BackupFolder::closeFile(int fHandle)
{
    int iResult = -1;
    if(fHandle > 0 && fHandle < MAX_NUM_SFTP_FILES)
    {
        if(m_fileList[fHandle] != NULL)
        {
            iResult = libssh2_sftp_close(m_fileList[fHandle]);
            m_fileList[fHandle] = NULL;
        }
    }
    return iResult;
}

int CSsh2BackupFolder::saveHandle(LIBSSH2_SFTP_HANDLE* fHandle)
{
    for(int i=1;i<MAX_NUM_SFTP_FILES;i++)
    {
        if(m_fileList[i] == NULL)
        {
            m_fileList[i] = fHandle;
            return i;
        }
    }
    //no place to save handle.
    libssh2_sftp_close(fHandle);
    return -1;
}

int CSsh2BackupFolder::createFile(const char* szRelativePath)
{
    string strPath = addToPath(m_strRootPath,szRelativePath);
    //create a file or if the file exists, truncate it.
    LIBSSH2_SFTP_HANDLE* fHandle = libssh2_sftp_open(m_pSftpSession, strPath.c_str(),LIBSSH2_FXF_WRITE|LIBSSH2_FXF_CREAT|LIBSSH2_FXF_TRUNC, LIBSSH2_SFTP_S_IRUSR|LIBSSH2_SFTP_S_IWUSR);  //this mode will be changed to original file's mode after closing.
    //no further processing necessary.
    if(fHandle != NULL)
        return saveHandle(fHandle);
    return -1;
}
int CSsh2BackupFolder::createFolder(const char* szRelativePath, mode_t mode)
{
    string strPath = addToPath(m_strRootPath,szRelativePath);
    LIBSSH2_SFTP_ATTRIBUTES sattr;
    int rc = libssh2_sftp_lstat(m_pSftpSession, strPath.c_str(),&sattr);
    if(rc == 0)    //dir/file exists.
    {
        if(sattr.permissions & LIBSSH2_SFTP_S_IFDIR)
        {
            //sftp_attributes_free(sattr);
            return 0;   //It exists and is a directory.
        }
        printf("A file/link by the name %s already exists. Cannot create directory.\r\n", strPath.c_str());
        //sftp_attributes_free(sattr);
        return CFD_CREATEDIR_FAIL;
    }
    //so it doesn't exist.
    //if(errno == EEXIST)//race condition?
    //    return 0;   //hope for the best. At worst, subsequent file operations will fail.
    int iResult = libssh2_sftp_mkdir(m_pSftpSession, strPath.c_str(), LIBSSH2_SFTP_S_IRUSR|LIBSSH2_SFTP_S_IWUSR|LIBSSH2_SFTP_S_IXUSR|LIBSSH2_SFTP_S_IRGRP|LIBSSH2_SFTP_S_IWGRP|LIBSSH2_SFTP_S_IXGRP|LIBSSH2_SFTP_S_IROTH|LIBSSH2_SFTP_S_IXOTH);  //we don't actually need to worry about upstream paths since they will all have to exist before we reach here.
    if(iResult == 0)
    {
        chmod(strPath.c_str(),mode);
        return 0;
    }
    printf("Error creating directory %s.\r\n", strPath.c_str());
    return CFD_CREATEDIR_FAIL;
}
int CSsh2BackupFolder::deleteFile(const char* szRelativePath)
{
    m_ztStats.m_uiNumFilesDeleted++;
    string strPath = addToPath(m_strRootPath,szRelativePath);
    int iResult = libssh2_sftp_unlink(m_pSftpSession, strPath.c_str());
    return iResult;
}

/*
int CSsh2BackupFolder::deleteFolder(const string& strRelativePath)
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
            libssh2_sftp_unlink(m_pSftpSession, strTemp.c_str());
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
    int iResult = libssh2_sftp_rmdir(m_pSftpSession, strPath.c_str());
    if(iResult)
        printf("Error %u removing folder %s.\r\n", iResult, strPath.c_str());
    return iResult;
}*/

int CSsh2BackupFolder::unlinkFile(const char* szRelativePath)
{
    string strTemp = addToPath(m_strRootPath,szRelativePath);
    int iResult = libssh2_sftp_unlink(m_pSftpSession, strTemp.c_str());
    return iResult;
}

int CSsh2BackupFolder::removeFolder(const char* szRelativePath)
{
    string strTemp = addToPath(m_strRootPath, szRelativePath);
    int iResult = libssh2_sftp_rmdir(m_pSftpSession, strTemp.c_str());
    return iResult;
}


time_t CSsh2BackupFolder::getAccTime(const char* szRelativePath)
{
    string strPath = addToPath(m_strRootPath, szRelativePath);
    LIBSSH2_SFTP_ATTRIBUTES sattr;
    
    int rc = libssh2_sftp_lstat(m_pSftpSession, strPath.c_str(), &sattr);
    if(rc == 0)
    {
        return sattr.atime;
    }
    return 0;
}
mode_t CSsh2BackupFolder::getFileMode(const char* szRelativePath)
{
    string strPath = addToPath(m_strRootPath, szRelativePath);
        
    LIBSSH2_SFTP_ATTRIBUTES sattr;
    int rc = libssh2_sftp_lstat(m_pSftpSession, strPath.c_str(),&sattr);
    if(rc == 0)
    {
        return (sattr.permissions & (LIBSSH2_SFTP_S_IRWXU|LIBSSH2_SFTP_S_IRWXG|LIBSSH2_SFTP_S_IRWXO));
    }
    return 0;
}
off64_t CSsh2BackupFolder::getFileSize(const char* szRelativePath)
{
    string strPath = addToPath(m_strRootPath, szRelativePath);
        
    LIBSSH2_SFTP_ATTRIBUTES sattr;
    int rc = libssh2_sftp_lstat(m_pSftpSession, strPath.c_str(),&sattr);
    if(rc == 0)
    {
        return sattr.filesize;
    }
    return 0;
}

int CSsh2BackupFolder::ensureRootAccess()
{
    string strPath = addToPath(m_strRootPath,"");
    LIBSSH2_SFTP_HANDLE* pFolder = libssh2_sftp_opendir(m_pSftpSession, strPath.c_str());
    if(pFolder == 0)
        return 0;
    libssh2_sftp_closedir(pFolder);
    return 1;
}


const char* CSsh2BackupFolder::getFirst(const char* szRelativePath)
{
    if(m_SftpDir)
    {
        libssh2_sftp_closedir(m_SftpDir);
        m_SftpDir = NULL;
    }
    m_strLastFoundPath.clear();
    m_lastFoundEntry.flags = 0;
    m_DirBuffer[0] = 0;
    
    string strPath = addToPath(m_strRootPath, szRelativePath);
    
    m_SftpDir = libssh2_sftp_opendir(m_pSftpSession, strPath.c_str());
    if(m_SftpDir == 0)
    {
        if(szRelativePath[0] == 0)  //this is the source/dest folder. we need to print an error message here
        {                           //since (unlike CLocalBackupFolder), we don't open the folder in 
                                    //InitializeToPath (and hence Initialize() wouldn't have caught the error.
            printf("Error opening SSH/SFTP source folder %s.\r\n",strPath.c_str());
        }
        return NULL;
    }
    
    return getNext(szRelativePath);
    
}
time_t CSsh2BackupFolder::getModTime(const char* szRelativePath)
{
    string strPath = addToPath(m_strRootPath, szRelativePath);
        
    LIBSSH2_SFTP_ATTRIBUTES sattr;
    int rc = libssh2_sftp_lstat(m_pSftpSession, strPath.c_str(),&sattr);
    if(rc == 0)
    {
        return sattr.mtime;
    }
    return 0;
}
const char* CSsh2BackupFolder::getNext(const char* szRelativePath)
{
    m_lastFoundEntry.flags = 0;
    m_DirBuffer[0] = 0;
    m_strLastFoundPath.clear();
    if(m_SftpDir == NULL)
        return NULL;

    int rc = libssh2_sftp_readdir(m_SftpDir,m_DirBuffer, DIR_BUF_MAXLEN, &m_lastFoundEntry);
    if(rc <= 0)
    {
        libssh2_sftp_closedir(m_SftpDir);
        m_SftpDir = NULL;
        return NULL;
    }
    //now file name is in m_DirBuffer.
    if(rc < DIR_BUF_MAXLEN) //otherwise, there is some issue.
    {
        m_DirBuffer[rc] = 0;
    }
    m_strLastFoundPath = addToPath(m_strRootPath,szRelativePath);
    m_strLastFoundPath += "/";
    m_strLastFoundPath += m_DirBuffer;
    return m_DirBuffer;
}
const char* CSsh2BackupFolder::getRootPath()
{
    return m_strRootPath.c_str();
}
int CSsh2BackupFolder::openFile(const char* szRelativePath)
{
    string strPath = addToPath(m_strRootPath,szRelativePath);
    //opens an existing file.
    LIBSSH2_SFTP_HANDLE* fHandle = libssh2_sftp_open(m_pSftpSession, strPath.c_str(),LIBSSH2_FXF_READ, LIBSSH2_SFTP_S_IRUSR|LIBSSH2_SFTP_S_IWUSR);  //this mode will be changed to original file's mode after closing.
    //no further processing necessary.
    if(fHandle != NULL)
        return saveHandle(fHandle);
    return -1;
}
off64_t CSsh2BackupFolder::readFile(int fHandle, char* dstBuffer, off64_t readCount)
{
    if(fHandle > 0 && fHandle < MAX_NUM_SFTP_FILES)
    {
        if(m_fileList[fHandle] != NULL)
        {
            ssize_t iRead = libssh2_sftp_read(m_fileList[fHandle], dstBuffer, readCount);
            //no processing necessary.
            return iRead;
        }
    }
    return 0;
}
int CSsh2BackupFolder::setFileMode(const char* szRelativePath, mode_t mode)
{
    //keep only file mode bits.
    mode &= (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);

    string strPath = addToPath(m_strRootPath, szRelativePath);

    LIBSSH2_SFTP_ATTRIBUTES sattr;
    int rc = libssh2_sftp_lstat(m_pSftpSession, strPath.c_str(),&sattr);
    if(rc == 0)
    {
        //first remove existing file modes.
        sattr.permissions &= 0xFFFFF800;
        //now add new ones.
        sattr.permissions |= mode;
        
        rc = libssh2_sftp_setstat(m_pSftpSession, strPath.c_str(), &sattr);
        return rc;
    }
    return rc;
}
int CSsh2BackupFolder::setModTime(const char* szRelativePath, time_t lastModTime, time_t lastAccTime)
{
    string strPath = addToPath(m_strRootPath, szRelativePath);

    LIBSSH2_SFTP_ATTRIBUTES sattr;
    int rc = libssh2_sftp_lstat(m_pSftpSession, strPath.c_str(),&sattr);
    if(rc == 0)
    {
        sattr.atime = lastAccTime;
        sattr.mtime = lastModTime;
        rc = libssh2_sftp_setstat(m_pSftpSession, strPath.c_str(), &sattr);
        return rc;
    }
    return rc;
}

int CSsh2BackupFolder::writeFile(int fHandle, const char* srcBuffer, off64_t writeCount)
{
    //in libssh2, write may not complete in one call. So, we loop until it is done.
    if(fHandle > 0 && fHandle < MAX_NUM_SFTP_FILES)
    {
        if(m_fileList[fHandle] != NULL)
        {
            int iDone = 0;
            do
            {
                ssize_t iWrite = libssh2_sftp_write(m_fileList[fHandle], &srcBuffer[iDone], writeCount);
                if(iWrite < 0)
                    return 1;//we don't know what happened. TODO: analyze SFTP errors and figure out the issue.
                iDone += iWrite;
                writeCount -= iWrite;
            }while(writeCount);
            return 0;
        }
    }
    return -1;
}
