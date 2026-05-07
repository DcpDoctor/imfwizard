#pragma once

#include <cstdio>

// Portable popen/pclose wrappers for Windows (MSVC uses _popen/_pclose)
#ifdef _WIN32
inline FILE* portable_popen(const char* cmd, const char* mode) { return _popen(cmd, mode); }
inline int portable_pclose(FILE* f) { return _pclose(f); }
#else
inline FILE* portable_popen(const char* cmd, const char* mode) { return popen(cmd, mode); }
inline int portable_pclose(FILE* f) { return pclose(f); }
#endif
