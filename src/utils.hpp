#include <filesystem>

#ifdef __MINGW32__
#   define PATHSTR(path) ((path).string())
#else
#   define PATHSTR(path) (path)
#endif
