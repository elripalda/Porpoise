/* Porpoise on Windows: the POSIX calls the shared code makes that MinGW
 * spells differently. Included before every file of the desktop build. */
#pragma once
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <sys/stat.h>
#ifdef __cplusplus
#include <cstdlib>
/* mkdir(path, mode): Windows has no modes. */
inline int mkdir(const char *path, int) { return _mkdir(path); }
#include <stdlib.h>
inline int setenv(const char *name, const char *value, int overwrite)
{
    if (!overwrite && getenv(name))
        return 0;
    return _putenv_s(name, value);
}
inline int unsetenv(const char *name) { return _putenv_s(name, ""); }
#include <time.h>
inline struct tm *localtime_r(const time_t *t, struct tm *out) { return localtime_s(out, t) == 0 ? out : nullptr; }
inline struct tm *gmtime_r(const time_t *t, struct tm *out) { return gmtime_s(out, t) == 0 ? out : nullptr; }
inline int fsync(int fd) { return _commit(fd); }
#endif
#endif
