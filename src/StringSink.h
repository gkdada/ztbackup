#pragma once

#include <string>

using namespace std;

//this is a class used to store strings.
class CStringSink
{
public:
    CStringSink(){m_pNext = NULL;};
    ~CStringSink(){  if(m_pNext)delete m_pNext;};
    void addString(const char* szString)
    {
        if(m_strString.length() == 0)
        {
            m_strString = szString;
            return;
        }
        if(m_pNext == NULL)
            m_pNext = new CStringSink;
        m_pNext->addString(szString);
    }
    void addString(string& szString)
    {
        if(m_strString.length() == 0)
        {
            m_strString = szString;
            return;
        }
        if(m_pNext == NULL)
            m_pNext = new CStringSink;
        m_pNext->addString(szString);
    }
    const string& getString(){return m_strString;};
    CStringSink* getNext(){return m_pNext;};
protected:
    string m_strString;
    CStringSink* m_pNext;
};


