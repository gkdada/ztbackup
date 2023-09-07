#include <memory.h>
#include <fnmatch.h>
#include "ExcludeList.h"
#include "BackupFolder.h"

#define EXCL_FILE_NAME ".ztexclude"

bool ExcludeList::s_bDebugMessages = false;

ExcludeList::ExcludeList(CBackupFolder* pParent, const std::string& strRelativePath)
{
    m_pParent = pParent;
    
    char Buffer[2010];
    
    string m_strFilePath = pParent->addToPath(strRelativePath, EXCL_FILE_NAME);
    int fHandle = pParent->openFile(m_strFilePath.c_str());
    if(fHandle == 0 || fHandle == -1)
        return;
    off64_t fSize = pParent->getFileSize(m_strFilePath.c_str());
    if(s_bDebugMessages)
    {
        printf("\rOpened a '.ztexclude' file at %s. Size = %llu\r\n", strRelativePath.c_str(), fSize);
    }
    off64_t iProcessed = 0, iLeftover = 0;
    while(iProcessed < fSize)
    {
        off64_t iToRead = fSize - iProcessed;
        if(iToRead > (2000 - iLeftover))
            iToRead = 2000 - iLeftover;
        off64_t iReadOctets = pParent->readFile(fHandle, &Buffer[iLeftover], iToRead);
        iProcessed += iReadOctets;
        Buffer[iReadOctets+iLeftover] = 0;
        
        processBuffer(Buffer, iReadOctets + iLeftover, iLeftover, iProcessed < fSize?false:true );
        
        if(iReadOctets != iToRead)    //we had an error reading. we stop reading more
            break;
    }
    
    pParent->closeFile(fHandle);
}

bool ExcludeList::isExcluded(const char* szName)
{
    if(m_PatternList.empty())
        return false;
    ExcludePatterns::iterator it = m_PatternList.begin();
    
    while(it != m_PatternList.end())
    {
        if(!fnmatch(it->c_str(), szName, 0))
        {
            if(s_bDebugMessages)
            {
                printf("\rNot backing up File/Folder '%s' since it matched the pattern '%s'\r\n", szName, it->c_str());
            }
            return true;
        }
        ++it;
    }
    
    
    
    return false;
}

void ExcludeList::processBuffer(char* caBuffer, off64_t iBufSize, off64_t& iLeftoverCount, bool bEndOfFile)
{
    off64_t iCur = 0;
    while(iCur < iBufSize)
    {
        off64_t i;
        for(i = iCur; i < iBufSize; i++)
        {
            if(caBuffer[i] == '\n' || caBuffer[i] == '\r')
                break;
        }
        if(i >= iBufSize) //end of buffer.
        {
            if((iBufSize - iCur) > 0)
            {
                //we have reached the end.
                if(bEndOfFile)
                {
                    addToList(&caBuffer[iCur], iBufSize-iCur);
                    //add the last piece
                    iLeftoverCount = 0;
                    return;
                }
                iLeftoverCount = iBufSize - iCur;
                memmove(caBuffer, &caBuffer[iCur], iLeftoverCount);
                return;
            }
        }
        //we have a line
        off64_t lineSize = i - iCur;
        addToList(&caBuffer[iCur], lineSize);
        //add the piece.
        iCur = i;
        while(caBuffer[iCur] == '\n' || caBuffer[iCur] == '\r')
            iCur++;
    }
    
}

void ExcludeList::addToList(const char* caBuffer, off64_t iLength)
{
    std::string strTemp = string(caBuffer, iLength);
    string_trim(strTemp);
    if(strTemp.length())
    {
        if(s_bDebugMessages)
        {
            printf("    Found a pattern '%s'\r\n", strTemp.c_str());
        }
        m_PatternList.push_back(strTemp);
    }
    
}

ExcludeList::~ExcludeList()
{
    
}