#pragma once

#include <algorithm> 
#include <cctype>
#include <locale>

#ifdef __APPLE__
typedef off_t off64_t;

// trim from start (in place)
static inline void string_ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            std::not1(std::ptr_fun<int, int>(std::isspace))));
}

// trim from end (in place)
static inline void string_rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
}

#else

// trim from start (in place)
static inline void string_ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void string_rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

#endif

// trim from both ends (in place)
static inline void string_trim(std::string &s) {
    string_ltrim(s);
    string_rtrim(s);
}

// trim from start (copying)
static inline std::string string_ltrim_copy(std::string s) {
    string_ltrim(s);
    return s;
}

// trim from end (copying)
static inline std::string string_rtrim_copy(std::string s) {
    string_rtrim(s);
    return s;
}

// trim from both ends (copying)
static inline std::string string_trim_copy(std::string s) {
    string_trim(s);
    return s;
}
