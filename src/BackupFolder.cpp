#include <unistd.h>
#include <string.h>
#include "BackupFolder.h"

//only defined here, since it is only used here.
int g_uiQueryTimeout = DEFAULT_QUERY_TIMER_LENGTH;

char g_SkipChars[] = "|/-\\";
int g_iCurrentSkipChar = 0;

int g_iCurrentCopyChar = 0;

//populated in main.cpp
bool g_bNoSymbolicLinks = false;


void PrintSkipping()
{
    printf("\33[2K\rSkipping%c",g_SkipChars[g_iCurrentSkipChar++ & 3]);
}

void PrintCopying(const char* szFilename, off64_t fCopied, off64_t fSize, bool bRestoring)
{
    off64_t Percent = (fCopied * 100)/fSize;
    if(bRestoring)
        printf("\33[2K\rRestoring %s %3lu%% %c",szFilename, (unsigned long)Percent, g_SkipChars[g_iCurrentCopyChar++ & 3]);
    else
        printf("\33[2K\rCopying %s %3lu%% %c",szFilename, (unsigned long)Percent, g_SkipChars[g_iCurrentCopyChar++ & 3]);
}

void PrintChecking()
{
    printf("\33[2K\rChecking for old files%c",g_SkipChars[g_iCurrentSkipChar++ & 3]);
}

void PrintDeleting()
{
    printf("\33[2K\rDeleting old files/folders%c",g_SkipChars[g_iCurrentSkipChar++ & 3]);
}

#define TWO_HOURS_TIME_T    7200    //3600 seconds per hour

CBackupFolder::CBackupFolder()
{
    m_eInitStatus = initNotDone;
}

CBackupFolder::~CBackupFolder()
{
}

string NumWithCommas(uint64_t Value)
{
    char striff[30];
    snprintf(striff, sizeof(striff), "%llu", Value);
    
    string numWithCommas = striff;
    int insertPosition = numWithCommas.length() - 3;
    while (insertPosition > 0)
    {
        numWithCommas.insert(insertPosition, ",");
        insertPosition-=3;
    } 
    return numWithCommas;
}

string CBackupFolder::getTimeStr()
{
    time_t tCur;
    time(&tCur);
    string strTime = asctime(localtime(&tCur));

    return strTime;
}

string CBackupFolder::getTimeStr(time_t tTimeVal)
{
    string strTime = asctime(gmtime(&tTimeVal));
    return strTime;
}


void CBackupFolder::printStats(uint32_t uiDestDeleteCount, CLogFile* pLogger)
{
    char PrintBuf[4000];
    
    size_t iCount = snprintf(PrintBuf,sizeof(PrintBuf), "Completed At %s\r\n",getTimeStr().c_str());
    
    iCount += snprintf(&PrintBuf[iCount],sizeof(PrintBuf)-iCount, "\r\nFolders traversed            %15s\r\n",NumWithCommas(m_ztStats.m_uiNumFolders).c_str());
    iCount += snprintf(&PrintBuf[iCount],sizeof(PrintBuf)-iCount, "Files skipped                %15s\r\n",NumWithCommas(m_ztStats.m_uiNumFilesSkipped).c_str());
    if(m_ztStats.m_ullSizeFilesSkipped)
        iCount += snprintf(&PrintBuf[iCount],sizeof(PrintBuf)-iCount, "Size of files skipped        %15s octets\r\n",NumWithCommas(m_ztStats.m_ullSizeFilesSkipped).c_str());
    iCount += snprintf(&PrintBuf[iCount],sizeof(PrintBuf)-iCount, "Files copied                 %15s\r\n",NumWithCommas(m_ztStats.m_uiNumFilesCopied).c_str());
    if(m_ztStats.m_ullSizeFilesCopied)
        iCount += snprintf(&PrintBuf[iCount],sizeof(PrintBuf)-iCount, "Size of files copied         %15s octets\r\n",NumWithCommas(m_ztStats.m_ullSizeFilesCopied).c_str());
    iCount += snprintf(&PrintBuf[iCount],sizeof(PrintBuf)-iCount, "Files restored               %15s\r\n",NumWithCommas(m_ztStats.m_uiNumFilesRestored).c_str());
    if(m_ztStats.m_ullSizeFilesRestored)
        iCount += snprintf(&PrintBuf[iCount],sizeof(PrintBuf)-iCount, "Size of files restored       %15s octets\r\n",NumWithCommas(m_ztStats.m_ullSizeFilesRestored).c_str());
    iCount += snprintf(&PrintBuf[iCount],sizeof(PrintBuf)-iCount, "Files deleted                %15s\r\n",NumWithCommas(uiDestDeleteCount).c_str());
    
    PrintBuf[iCount] = 0;
    
    printf("%s",PrintBuf);
    pLogger->AddToLog(PrintBuf);
}

INIT_STATUS CBackupFolder::Initialize(const char* szRootFolder, bool bAnonymousAccess, CBackupFolder* pSrc)
{
    if(m_eInitStatus != initNotDone)
    {
        printf("Fatal error: already initialized! Status = %u\r\n",m_eInitStatus);
        return m_eInitStatus;
    }
    m_strRootPath = szRootFolder;
    m_eInitStatus = InitializeToPath(szRootFolder, bAnonymousAccess, pSrc);
    if(m_eInitStatus == initSuccess)
    {
        if(ensureRootAccess())
            return initSuccess;
        printf("Error accessing root folder for %s",(pSrc == NULL)?"Source":"Destination");
        return initRootNotFound;
    }
    return m_eInitStatus;
}


int CBackupFolder::StartBackup(CBackupFolder* pDest, FILE_OPTION eFileOption, FOLDER_OPTION eFolderOption, bool bRecursive, CLogFile* pLogger)
{
    string strPath;
    
    char szLogString[2000];
    
    snprintf(szLogString, sizeof(szLogString), "\r\nInitiating zero-touch Backup at %s\r\n",getTimeStr().c_str());
    pLogger->AddToLog(szLogString);
    snprintf(szLogString, sizeof(szLogString), "Source Folder: %s\r\n",m_strRootPath.c_str());
    pLogger->AddToLog(szLogString);
    snprintf(szLogString, sizeof(szLogString), "Destination Folder: %s\r\n\r\n",pDest->m_strRootPath.c_str());
    pLogger->AddToLog(szLogString);

    if(eFileOption == fileOptionNotSet)
        eFileOption = fileOptionAsk;
    if(eFolderOption == folderOptionNotSet)
        eFolderOption = folderOptionAsk;
    
    //start with empty string to specify starting folder.
    return BackupThis(strPath, pDest, eFileOption, eFolderOption, bRecursive);
} 
int CBackupFolder::BackupThis(const string& strRelativePath, CBackupFolder* pDest, FILE_OPTION eFileOption, FOLDER_OPTION eFolderOption, bool bRecursive)
{
    
    if(strRelativePath.length())    //if this is the root folder, we will mark it traversed only if we succeed in opening it.
        m_ztStats.m_uiNumFolders++;
    
    int iResult;
    //1. parse all the files and folders.
        //1a. if file, compare and (if necessary) update/copy/restore the file over.
        //1b. if folder & recursive is set, save to the list of folders.
    CStringSink srcDirs, dstDirs;
    const char* szItem = getFirst(strRelativePath.c_str());
    if(!strRelativePath.length())
    {
        if(szItem == NULL)  //probably error accessing the entire source folder. Do not continue.
        {
            printf("Error accessing the source folder. Aborting the backup process.\r\n");
            return -1;
        }
        m_ztStats.m_uiNumFolders++; //so, we have at least one item in the root folder.
    }
    else
        printf("\33[2K\rEntering folder: %s\r\n",strRelativePath.c_str());
    ExcludeList xList(this, strRelativePath.c_str());
    
    if(szItem != NULL)//there is atleast one file or folder in the source directory.
    {
        int iResult = pDest->createFolder(strRelativePath.c_str(),getFileMode(strRelativePath.c_str()));
        if(iResult != 0)
        {
            if(strRelativePath.length() == 0)
            {
                //error accessing the destination folder. display it.
                printf("Error accessing the destination folder. Aborting the backup process.\r\n");
                return -1;
            }
            return 1;//cannot backup this folder (or its subfolders) but otherwise, backup can continue.
        }
    }
    while(szItem != NULL)
    {
        if(IsItALink() == false || g_bNoSymbolicLinks == false) //if g_bNoSymbolicLinks is defined and this is a symbolic link, skip it.
        {
            string itemPathRelative = addToPath(strRelativePath,szItem);
            if(IsItAFile())
            {
                if(xList.isExcluded(szItem) == false)
                {
                    time_t dstModTime = pDest->getModTime(itemPathRelative.c_str());
                    if(dstModTime == 0)//file doesn't exist.
                    {
                        iResult = CopyFileToDest(itemPathRelative, pDest);
                        if(iResult < 0)
                            return iResult;
                    }
                    else //compare the size and mod time.
                    {
                        if(getFileSize(itemPathRelative.c_str()) != pDest->getFileSize(itemPathRelative.c_str())) //size is different.
                        {
                            time_t srcModTime = getModTime(itemPathRelative.c_str());
                            if(srcModTime < dstModTime)
                            {
                                //Destination is newer. Ask whether to restore it or backup or leave the existing backup a
                                char actionChar = AskTimedRestoreQuestion(itemPathRelative.c_str(), pDest);
                                switch(actionChar)
                                {
                                    case 'l':
                                        m_ztStats.m_uiNumFilesSkipped++;
                                        m_ztStats.m_ullSizeFilesSkipped += getFileSize(itemPathRelative.c_str());
                                        PrintSkipping();
                                        break;  //nothing to do.
                                    case 'b':
                                        iResult = CopyFileToDest(itemPathRelative, pDest);
                                        if(iResult < 0)
                                            return iResult;
                                        break;
                                    case 'r':
                                        iResult = pDest->CopyFileToDest(itemPathRelative, this, true);
                                        if(iResult < 0)
                                            return iResult;
                                        break;
                                }
                            }
                            else
                            {
                                iResult = CopyFileToDest(itemPathRelative, pDest);
                            }
                            if(iResult < 0)
                                return iResult;
                        }
                        else //compare mod time.
                        {
                            time_t srcModTime = getModTime(itemPathRelative.c_str());
                            //don't copy if within two hours. this will eliminate file system differences
                            //as well as time zone implementation differences.
                            if(abs(srcModTime - dstModTime) < TWO_HOURS_TIME_T)
                            {
                                m_ztStats.m_uiNumFilesSkipped++;
                                m_ztStats.m_ullSizeFilesSkipped += getFileSize(itemPathRelative.c_str());
                                PrintSkipping();
                            }
                            else if(srcModTime < dstModTime)
                            {
                                //Destination is newer. Ask whether to restore it or backup or leave the existing backup a
                                char actionChar = AskTimedRestoreQuestion(itemPathRelative.c_str(), pDest);
                                switch(actionChar)
                                {
                                    case 'l':
                                        m_ztStats.m_uiNumFilesSkipped++;
                                        m_ztStats.m_ullSizeFilesSkipped += getFileSize(itemPathRelative.c_str());
                                        PrintSkipping();
                                        break;  //nothing to do.
                                    case 'b':
                                        iResult = CopyFileToDest(itemPathRelative, pDest);
                                        if(iResult < 0)
                                            return iResult;
                                        break;
                                    case 'r':
                                        iResult = pDest->CopyFileToDest(itemPathRelative, this, true);
                                        if(iResult < 0)
                                            return iResult;
                                        break;
                                }
                            }
                            else    //so the source is newer. copy to destination.
                            {
                                iResult = CopyFileToDest(itemPathRelative, pDest);
                                if(iResult < 0)
                                    return iResult;
                            }
                        }
                    }
                }
            }
            else if (IsItADir())
            {
                if(xList.isExcluded(szItem) == false)
                {
                    if(bRecursive)
                    {
                        //don't add "." & ".."
                        if(strcmp(szItem,".") && strcmp(szItem,".."))
                            srcDirs.addString(itemPathRelative);
                    }
                }
            }
        }
        szItem = getNext(strRelativePath.c_str());
    }
            
    //2. for each folder in list,
        //2a. recurse to backup the folder.
    if(bRecursive)
    {
        CStringSink* pCur = &srcDirs;
        while(pCur)
        {
            const char* pTemp = pCur->getString().c_str();
            if(pTemp[0] == 0)    //no more directories.
                break;
            //string strTemp = addToPath(strRelativePath,pTemp);
            iResult = BackupThis(pCur->getString(), pDest, eFileOption, eFolderOption, bRecursive);
            if(iResult < 0)
                return iResult;
            
            pCur = pCur->getNext();
        }
    }
    
    //3. if fileoption and/or folderoption are set to ask or delete,
        //3a. parse all files & folders in destination and offer to delete/restore any that don't have counterparts in source.
            //3aa. if file, compare and (if necessary), delete/restore the file in source.
            //3ab. if folder, save to list of folders.
    if(eFileOption != fileOptionLeave && eFolderOption != folderOptionLeave)
    {
        PrintChecking();
        const char* szItem = pDest->getFirst(strRelativePath.c_str());
        while(szItem != NULL)
        {
            string itemPathRelative = addToPath(strRelativePath,szItem);
            if(pDest->IsItAFile())
            {
                if(eFileOption != fileOptionLeave)
                {
                    if(getModTime(itemPathRelative.c_str()) == 0)
                    {
                        char actionChar;
                        switch(eFileOption)
                        {
                            case fileOptionAsk:
                                actionChar = AskTimedDeleteQuestion(itemPathRelative.c_str(), true);
                                printf("\r\n");
                                break;
                            case fileOptionDelete:
                                actionChar = 'd';
                                break;
                            default:
                                actionChar = 'l';
                                break;
                        }
                        switch(actionChar)
                        {
                            case 'l':
                                break;  //nothing to do.
                            case 'd':
                                pDest->deleteFile(itemPathRelative.c_str());
                                break;
                            case 'r':
                                iResult = pDest->CopyFileToDest(itemPathRelative, this, true);
                                if(iResult < 0)
                                    return iResult;
                                break;
                        }
                    }
                }
            }
            else if (pDest->IsItADir())
            {
                if(strcmp(szItem,".") && strcmp(szItem,".."))
                {
                    if(eFolderOption != folderOptionLeave)
                    {
                        if(getModTime(itemPathRelative.c_str()) == 0)
                        {
                            char actionChar;
                            switch(eFolderOption)
                            {
                                case folderOptionAsk:
                                    actionChar = AskTimedDeleteQuestion(itemPathRelative.c_str(), false);
                                    printf("\r\n");
                                    break;
                                case folderOptionDelete:
                                    actionChar = 'd';
                                    break;
                                default:
                                    actionChar = 'l';
                                    break;
                            }
                            switch(actionChar)
                            {
                                case 'l':
                                    break;  //nothing to do.
                                case 'd':
                                    pDest->deleteFolder(itemPathRelative);
                                    printf("\r\n");
                                    szItem = NULL; //this signals the code below that we need to do a getFirst again.
                                    break;
                                case 'r':
                                    //this could be just a call to BackupThis in reverse.
                                    //but I have decided to implement this separately to keep it simple.
                                    iResult = pDest->CopyNewFolderToDest(itemPathRelative, this, true);
                                    if(iResult < 0)
                                        return iResult;
                                    break;
                            }
                        }
                    }
                }
            }
            if(szItem)
                szItem = pDest->getNext(strRelativePath.c_str());
            else
                szItem = pDest->getFirst(strRelativePath.c_str());//deleteFolder routine would have upset the getFirst/getNext handles.
        }
        
    }

        //3b. Parse through list of folders and delete 
    return 0;
}

int CBackupFolder::CopyNewFolderToDest(const string& strRelativePath, CBackupFolder* pDest, bool bRestoring)
{
    int iResult;
    //1. parse all the files and folders.
        //1a. if file, compare and (if necessary) update/copy/restore the file over.
        //1b. if folder & recursive is set, save to the list of folders.
    CStringSink srcDirs;
    const char* szItem = getFirst(strRelativePath.c_str());
    if(szItem != NULL)//there is atleast one file or folder in the source directory.
        pDest->createFolder(strRelativePath.c_str(), getFileMode(strRelativePath.c_str()));
    while(szItem != NULL)
    {
        string itemPathRelative = addToPath(strRelativePath,szItem);
        if(IsItAFile())
        {
            iResult = CopyFileToDest(itemPathRelative, pDest, bRestoring);
            if(iResult < 0)
                return iResult;
        }
        else if (IsItADir())
        {
            srcDirs.addString(itemPathRelative);
        }
        szItem = getNext(strRelativePath.c_str());
    }
    
    CStringSink* pCur = &srcDirs;
    while(pCur)
    {
        const char* pTemp = pCur->getString().c_str();
        if(pTemp[0] == 0)    //no more directories.
            break;
        //string strTemp = addToPath(strRelativePath,pTemp);
        iResult = CopyNewFolderToDest(pCur->getString(), pDest, bRestoring);
        if(iResult < 0)
            return iResult;
        
        pCur = pCur->getNext();
    }
    return 0;
}


int CBackupFolder::CopyFileToDest(const string& strRelativePath, CBackupFolder* pDest, bool bRestoring)
{
    if(bRestoring)
        printf("\33[2K\rRestoring %s to %s",strRelativePath.c_str(),pDest->getRootPath());
    else
        printf("\33[2K\rCopying %s",strRelativePath.c_str());
    char Buffer[512000];
    
    off64_t fSize = getFileSize(strRelativePath.c_str());
    int srcFile = openFile(strRelativePath.c_str());
    if(srcFile == 0 || srcFile == -1)
    {
        printf("\r\nError opening source file %s\r\n",strRelativePath.c_str());
        return CFD_OPEN_FAIL;
    }
    
    int dstFile = pDest->createFile(strRelativePath.c_str());
    if(dstFile == 0 || dstFile == -1)
    {
        printf("\r\nError opening destination file %s\r\n", strRelativePath.c_str());
        return CFD_CREATE_FAIL;
    }
    
    off64_t copied = 0;
    while(copied < fSize)
    {
        off64_t toRead = fSize - copied;
        if(toRead > 512000)
            toRead = 512000;
        off64_t readOctets = readFile(srcFile, Buffer, toRead);
        if(readOctets < 0)
        {
            printf("\r\nRead failed for %s. Cannot copy.\r\n", strRelativePath.c_str());
            return CFD_READ_FAIL;
        }
        //number of bytes read could be lower than requested. But that's ok.
        int wResult = pDest->writeFile(dstFile, Buffer, readOctets);
        if(wResult)
        {
            printf("\r\nError writing to %s in %s.%s\r\n",strRelativePath.c_str(), bRestoring?"source":"target",(wResult == CFD_DEST_FULL)?" Disk may be full.":"");
            return wResult;
        }
            
        copied += readOctets;
        //printf(".");
        PrintCopying(strRelativePath.c_str(), copied, fSize, bRestoring);
        
    }
    
    closeFile(srcFile);
    pDest->closeFile(dstFile);
    
    if(bRestoring)
    {
        pDest->m_ztStats.m_uiNumFilesRestored++;
        pDest->m_ztStats.m_ullSizeFilesRestored += fSize;
    }
    else
    {
        m_ztStats.m_uiNumFilesCopied++;
        m_ztStats.m_ullSizeFilesCopied += fSize;
    }
    
    printf("\r\n");
    
    //assign permissions and date.
    mode_t mode = getFileMode(strRelativePath.c_str());
    //keep only file mode bits.
    mode &= (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
    if(mode)
        pDest->setFileMode(strRelativePath.c_str(), mode);
    
    //also date.
    time_t modTime = getModTime(strRelativePath.c_str());
    time_t accTime = getAccTime(strRelativePath.c_str());
    if(modTime)
        pDest->setModTime(strRelativePath.c_str(), modTime, accTime);
    
    return 0;
}

int CBackupFolder::deleteFolder(const string& strRelativePath)
{
    //we need to delete ALL files & all sub-folders.
    CStringSink dirList;
    const char* fName = getFirst(strRelativePath.c_str());
    while(fName != NULL)
    {
        PrintDeleting();
        
        if(IsItADir())
        {
            if(strcmp(fName,".") && strcmp(fName,".."))
                dirList.addString(fName);
        }
        else //otherwise, just delete it. we don't care if it is a symbolic link or anything.
        {
            m_ztStats.m_uiNumFilesDeleted++;
            string strTemp = strRelativePath + "/" + fName;
            unlinkFile(strTemp.c_str());
            //strTemp += "/";
            //strTemp += fName;
            //libssh2_sftp_unlink(m_pSftpSession, strTemp.c_str());
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
    //int iResult = libssh2_sftp_rmdir(m_pSftpSession, strPath.c_str());
    int iResult = removeFolder(strRelativePath.c_str());
    if(iResult)
    {
        string strPath = addToPath(m_strRootPath,strRelativePath.c_str());
        printf("Error %u removing folder %s.\r\n", iResult, strPath.c_str());
    }
    return iResult;
}


int CBackupFolder::KeyPressWaiting()
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

//returns 'l' for "Leave", 'd' for "Delete", or 'r' for "Restore"
//bIsAFile should be true to indicate a file and false to indicate a folder 
char CBackupFolder::AskTimedDeleteQuestion(const char* szRelativePath, bool bIsAFile)
{
    const char* szItemType = (bIsAFile)?"file":"folder";
    printf("The source for the backed up %s '%s' doesn't exist anymore.\r\n",szItemType, szRelativePath);
    
    char szQueryString[200];
    snprintf(szQueryString, sizeof(szQueryString), "Do you want to (d)elete, (r)estore, or (l)eave the %s?",szItemType);
    
    useconds_t usecs = DEFAULT_SLEEP_LEN;
    char c;
    int iCur = 0;
    while(iCur < g_uiQueryTimeout)
    {
        usleep(usecs);
        iCur += 20; //was asleep for 20 milliseconds
        if(KeyPressWaiting())
        {
            c = getchar();
            switch(c)
            {
                case 'l':
                case 'L':
                case '\n':
                    g_uiQueryTimeout = DEFAULT_QUERY_TIMER_LENGTH; //reset query timer.
                    return 'l';
                case 'd':
                case 'D':
                    g_uiQueryTimeout = DEFAULT_QUERY_TIMER_LENGTH;
                    return 'd';
                case 'r':
                case 'R':
                    g_uiQueryTimeout = DEFAULT_QUERY_TIMER_LENGTH;
                    return 'r';
                default:
                    break;
            }
        }
        printf("\33[2K\r%s [l] in %3u seconds:",szQueryString,(g_uiQueryTimeout-iCur)/1000);
    }
    
    g_uiQueryTimeout /= 2;  //cut query timer in half - with a minimum of 6 seconds.
    if(g_uiQueryTimeout < MIN_QUERY_TIMER_LENGTH)
        g_uiQueryTimeout = MIN_QUERY_TIMER_LENGTH;
    return '1';//for Leave.
}

//returns 'l' for "Leave", 'b' for "backup", or 'r' for "Restore"
char CBackupFolder::AskTimedRestoreQuestion(const char* szRelativePath, CBackupFolder* pDest)
{
    printf("The destination for the backed up file '%s' is newer than the source.\r\n\r\n", szRelativePath);
    
    setlocale(LC_NUMERIC, "");    
    printf("             size (bytes)               modified time\r\n");
    time_t ModTime = getModTime(szRelativePath);
    off64_t ModSize = getFileSize(szRelativePath);
    printf("source:      %'-26llu %s", ModSize, getTimeStr(ModTime).c_str());
    ModTime = pDest->getModTime(szRelativePath);
    ModSize = pDest->getFileSize(szRelativePath);
    printf("destination: %'-26llu %s\r\n", ModSize, getTimeStr(ModTime).c_str());
    
    char szQueryString[200];
    snprintf(szQueryString, sizeof(szQueryString), "Do you want to (b)ackup, (r)estore, or (l)eave the file?");
    
    useconds_t usecs = DEFAULT_SLEEP_LEN;
    char c;
    int iCur = 0;
    while(iCur < g_uiQueryTimeout)
    {
        usleep(usecs);
        iCur += 20; //was asleep for 20 milliseconds
        if(KeyPressWaiting())
        {
            c = getchar();
            switch(c)
            {
                case 'l':
                case 'L':
                case '\n':
                    g_uiQueryTimeout = DEFAULT_QUERY_TIMER_LENGTH; //reset query timer.
                    return 'l';
                case 'b':
                case 'B':
                    g_uiQueryTimeout = DEFAULT_QUERY_TIMER_LENGTH;
                    return 'b';
                case 'r':
                case 'R':
                    g_uiQueryTimeout = DEFAULT_QUERY_TIMER_LENGTH;
                    return 'r';
                default:
                    break;
            }
        }
        printf("\33[2K\r%s [l] in %3u seconds:",szQueryString,(g_uiQueryTimeout-iCur)/1000);
    }
    
    g_uiQueryTimeout /= 2;  //cut query timer in half - with a minimum of 6 seconds.
    if(g_uiQueryTimeout < MIN_QUERY_TIMER_LENGTH)
        g_uiQueryTimeout = MIN_QUERY_TIMER_LENGTH;
    return '1';//for Leave.
}


string CBackupFolder::addToPath(const string& strCurrent, const char* szAddend)
{
    string strNew;
    if(strCurrent.length())
    {
        strNew = strCurrent;
        if(szAddend[0])
            strNew += "/";
    }
    strNew += szAddend;
    return strNew;
}

string CBackupFolder::addToPath(const char* szCurrent, const char* szAddend)
{
    string strNew;
    if(szCurrent[0])
    {
        strNew = szCurrent;
        if(szAddend[0])
            strNew += "/";
    }
    strNew += szAddend;
    return strNew;
}
