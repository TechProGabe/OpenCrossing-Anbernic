#ifndef MSL_CTYPE_H
#define MSL_CTYPE_H

#ifdef TARGET_PC
/* On PC use the system ctype.h — __declspec(weak) is MSVC/MinGW-only,
 * and the original _CTYPE_H guard collides with glibc's system header. */
#include <ctype.h>
/* Underscore aliases used by game code via MSL inlines */
#define _isalpha(c)  isalpha(c)
#define _isdigit(c)  isdigit(c)
#define _isspace(c)  isspace(c)
#define _isupper(c)  isupper(c)
#define _isxdigit(c) isxdigit(c)
#define _tolower(c)  tolower(c)
#define _toupper(c)  toupper(c)
#else

#include "MSL_C/locale.h"
#include "MSL_C/ctype_api.h"

#ifdef __cplusplus
extern "C" {
#endif

__declspec(weak) int isalpha(int __c);
__declspec(weak) int isdigit(int __c);
__declspec(weak) int isspace(int __c);
__declspec(weak) int isupper(int __c);
__declspec(weak) int isxdigit(int __c);

__declspec(weak) int tolower(int __c);
__declspec(weak) int toupper(int __c);

// added underscore to avoid naming conflicts
inline int _isalpha(int c) {
    return (int)(__ctype_map[(unsigned char)c] & __letter);
}
inline int _isdigit(int c) {
    return (int)(__ctype_map[(unsigned char)c] & __digit);
}
inline int _isspace(int c) {
    return (int)(__ctype_map[(unsigned char)c] & __whitespace);
}
inline int _isupper(int c) {
    return (int)(__ctype_map[(unsigned char)c] & __upper_case);
}
inline int _isxdigit(int c) {
    return (int)(__ctype_map[(unsigned char)c] & __hex_digit);
}
inline int _tolower(int c) {
    return (c == -1 ? -1 : (int)__lower_map[(unsigned char)c]);
}
inline int _toupper(int c) {
    return (c == -1 ? -1 : (int)__upper_map[(unsigned char)c]);
}

#ifdef __cplusplus
}
#endif
#endif /* !TARGET_PC */
#endif /* MSL_CTYPE_H */
