#pragma once

#include <cstdio>
#include <ctime>

// Portable popen/pclose wrappers for Windows (MSVC uses _popen/_pclose)
#ifdef _WIN32
inline FILE* portable_popen(const char* cmd, const char* mode)
{
  return _popen(cmd, mode);
}
inline int portable_pclose(FILE* f)
{
  return _pclose(f);
}
#else
inline FILE* portable_popen(const char* cmd, const char* mode)
{
  return popen(cmd, mode);
}
inline int portable_pclose(FILE* f)
{
  return pclose(f);
}
#endif

// Portable gmtime_r (MSVC uses gmtime_s with reversed arguments)
inline struct tm* portable_gmtime(const time_t* timer, struct tm* buf)
{
#ifdef _WIN32
  return gmtime_s(buf, timer) == 0 ? buf : nullptr;
#else
  return gmtime_r(timer, buf);
#endif
}
