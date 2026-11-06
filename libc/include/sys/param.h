#ifndef _SYS_PARAM_H
#define _SYS_PARAM_H 1
#include <limits.h>
#define MAXPATHLEN PATH_MAX
#define MAXHOSTNAMELEN 64
#define NBBY 8
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#endif
