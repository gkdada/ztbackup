#include <unistd.h>
#include <termios.h>            //termios, TCSANOW, ECHO, ICANON
#include <config.h>
//#include "BackupRoutine.h"
#include "LocalBackupFolder.h"
#include "SmbBackupFolder.h"
//#include "SshBackupFolder.h"
#include "Ssh2BackupFolder.h"

#define BUILD_YEAR_CH0 (__DATE__[ 7])
#define BUILD_YEAR_CH1 (__DATE__[ 8])
#define BUILD_YEAR_CH2 (__DATE__[ 9])
#define BUILD_YEAR_CH3 (__DATE__[10])

//defined in SmbBackupFolder.cpp
extern bool g_bUseAnonymousAccess;
//defined in BackupFolder.cpp
extern bool g_bNoSymbolicLinks;

struct termios g_SavedTerminal;

void ModTerminal()
{
    termios newt;
    
    /*tcgetattr gets the parameters of the current terminal
    STDIN_FILENO will tell tcgetattr that it should write the settings
    of stdin to oldt*/
    tcgetattr( STDIN_FILENO, &g_SavedTerminal);
    /*now the settings will be copied*/
    newt = g_SavedTerminal;

    /*ICANON normally takes care that one line at a time will be processed
    that means it will return if it sees a "\n" or an EOF or an EOL*/
    newt.c_lflag &= ~(ICANON);          

    /*Those new settings will be set to STDIN
    TCSANOW tells tcsetattr to change attributes immediately. */
    tcsetattr( STDIN_FILENO, TCSANOW, &newt);
    
}

void RestoreSavedTerminal()
{
   /*restore the old settings*/
    tcsetattr( STDIN_FILENO, TCSANOW, &g_SavedTerminal);
}


int main(int argc, char **argv)
{
    setbuf(stdout, NULL);
    
    string strLogDir;
    string SrcFolder,DstFolder;
    
    
    fileOption FileOpt = fileOptionNotSet;
    folderOption FoldOpt = folderOptionNotSet;
    bool bRecursive = false;

    //first, open the log file. By default, we store it in ~/.ztbackup
    char* pHomeDir = getenv("HOME");
    if(pHomeDir == NULL)
        strLogDir = "~/.ztbackup";
    else
    {
        strLogDir = pHomeDir;
        strLogDir += "/.ztbackup";
    }
    
    //just create the folder. we are ok if this call fails.
    mkdir(strLogDir.c_str(),S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IEXEC|S_IROTH);
    
    CLogFile mLogger(strLogDir.c_str());
    
    char szLogString[1000];
    

    snprintf(szLogString,sizeof(szLogString), "Zero-touch Backup command-line tool Ver. %s (built %s). Copyright (c) gkdada 2008-%c%c%c%c.\r\n",PACKAGE_VERSION,__DATE__,BUILD_YEAR_CH0,BUILD_YEAR_CH1,BUILD_YEAR_CH2,BUILD_YEAR_CH3);
    printf("%s",szLogString);
    mLogger.AddToLog(szLogString);

    for(int i = 1; i < argc ; i++ )
    {
        if(argv[i][0] == '-')
        {
            if(argv[i][1] != '-')
            {
                int j = 1;
                while(argv[i][j])
                {
                    if(argv[i][j] == 'l')
                    {
                        if(FileOpt == fileOptionNotSet)
                            FileOpt = fileOptionLeave;
                        else
                        {
                            printf("Error: Multiple 'File' options specified.\r\n");
                            return 0;
                        }
                    }
                    if(argv[i][j] == 'm')
                    {
                        if(FoldOpt == folderOptionNotSet)
                            FoldOpt = folderOptionLeave;
                        else
                        {
                            printf("Error: Multiple 'Folder' options specified.\r\n");
                            return 0;
                        }
                    }
                    if(argv[i][j] == 'a')
                    {
                        if(FileOpt == fileOptionNotSet)
                            FileOpt = fileOptionAsk;
                        else
                        {
                            printf("Error: Multiple 'File' options specified.\r\n");
                            return 0;
                        }
                    }
                    if(argv[i][j] == 'b')
                    {
                        if(FoldOpt == folderOptionNotSet)
                            FoldOpt = folderOptionAsk;
                        else
                        {
                            printf("Error: Multiple 'Folder' options specified.\r\n");
                            return 0;
                        }
                    }
                    if(argv[i][j] == 'd')
                    {
                        if(FileOpt == fileOptionNotSet)
                            FileOpt = fileOptionDelete;
                        else
                        {
                            printf("Error: Multiple 'File' options specified.\r\n");
                            return 0;
                        }
                    }
                    if(argv[i][j] == 'e')
                    {
                        if(FoldOpt == folderOptionNotSet)
                            FoldOpt = folderOptionDelete;
                        else
                        {
                            printf("Error: Multiple 'Folder' options specified.\r\n");
                            return 0;
                        }
                    }
                    if(argv[i][j] == 'r')
                    {
                        bRecursive = true;
                    }
                    if(argv[i][j] == 'u')
                    {
                        g_bUseAnonymousAccess = true;
                    }
                    if(argv[i][j] == 'n')
                    {
                        g_bNoSymbolicLinks = true;
                    }
                    if(argv[i][j] == '1')
                    {
                        //debug LocalBackupFolder
                    }
                    if(argv[i][j] == '2')
                    {
                        //debug SmbBackupFolder
                    }
                    if(argv[i][j] == '3')
                    {
                        //debug SshBackupFolder
                    }
                    if(argv[i][j] == '4')
                    {
                        //debug ExcludeList
                        ExcludeList::PrintDebugMessages();
                    }
                    
                    j++;
                }
            }
            else //handle --options here.
            {
            }
        }
        else
        {
            //must be source or destination folder, in that order.
            if(SrcFolder.length() == 0)
            {
                SrcFolder = argv[i];
            }
            else if(DstFolder.length() == 0)
            {
                DstFolder = argv[i];
            }
            else
            {
                printf("unknown argument '%s'\r\n",argv[i]);
                return 0;
            }
        }
    }
    
    
    //save the terminal settings and reset it to allow key input.
    ModTerminal();
    
    if(SrcFolder.length() == 0)
    {
        printf("Source folder not specified for backup.\r\n");
        return 0;
    }
    if(DstFolder.length() == 0)
    {
        printf("Destination folder not specified for backup.\r\n");
        return 0;
    }

    char szCurFolder[FILENAME_MAX];
    getcwd(szCurFolder,FILENAME_MAX);

    printf("CurrentFolder: %s\r\nSource folder: %s\r\nDestination folder: %s\r\n",szCurFolder, SrcFolder.c_str(), DstFolder.c_str());
 
    
    
    //restore old terminal back at exit.
    atexit(RestoreSavedTerminal);
    
    LPBACKUP_FOLDER pSrcFolder = NULL, pDestFolder = NULL;
    
    if(!SrcFolder.compare(0,6,"smb://"))
        pSrcFolder = new CSmbBackupFolder();
    else if(!SrcFolder.compare(0,6,"ssh://"))
        pSrcFolder = new CSsh2BackupFolder();
    else
        pSrcFolder = new CLocalBackupFolder();
    
    
    if(!DstFolder.compare(0,6,"smb://"))
        pDestFolder = new CSmbBackupFolder();
    else if(!DstFolder.compare(0,6,"ssh://"))
        pDestFolder = new CSsh2BackupFolder();
    else
        pDestFolder = new CLocalBackupFolder();
    
   
    //CLocalBackupFolder sFolder,dFolder;
    
    if(pSrcFolder->Initialize(SrcFolder.c_str(), g_bUseAnonymousAccess, NULL))
    {
        printf("Error initializing source folder.\r\n");
        return 0;
    }
    if(pDestFolder->Initialize(DstFolder.c_str(), g_bUseAnonymousAccess, pSrcFolder))
    {
        printf("Error initializing destination folder.\r\n");
        return 0;
    }
    
    pSrcFolder->StartBackup(pDestFolder,FileOpt, FoldOpt, bRecursive, &mLogger);
    
    pSrcFolder->printStats(pDestFolder->getDeletedCount(), &mLogger);
    //original debug arguments
    //-abr smb://Server72/Software /media/gsagar/Backup-2/Software
    
//    CBackupRoutine bkup;
    
    //bkup.SshBackup(SrcFolder, DstFolder, FileOpt, FoldOpt, bRecursive);

	return 0;
}
