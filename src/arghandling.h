#include <stdbool.h>

#define ARRAYELTS(arr) (sizeof (arr) / sizeof (arr)[0])

#ifdef __cplusplus
extern "C" {
#endif

bool
argmatch (char **, int, const char *, const char *, int, int *, int *);

#ifdef __cplusplus
}
#endif
