#ifndef __VPMU_PATH_LIB_H_
#define __VPMU_PATH_LIB_H_

#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // access()
#include <stdbool.h> // bool, true, false
#include <ctype.h>   // isspace()
#include <libgen.h>  // basename(), dirname(), etc.
#include <fnmatch.h> // fnmatch()
#include <glob.h>    // glob()

#ifndef DBG_MSG
#define DBG_MSG(str, ...)                                                                \
    do {                                                                                 \
        if (getenv("DEBUG") != NULL) printf(str, ##__VA_ARGS__);                         \
    } while (0)
#endif

static inline bool startwith(const char *str, const char *pre)
{
    return strncmp(pre, str, strlen(pre)) == 0;
}

static inline bool endwith(const char *str, const char *pro)
{
    int shift = strlen(str) - strlen(pro);
    if (shift < 0) return false;
    return strncmp(pro, str + shift, strlen(pro)) == 0;
}

static inline int isquote(char c)
{
    return (c == '"' || c == '\'');
}

static inline void emplace_trim(char *s)
{
    char *p = s;
    int   l = strlen(p);

    while (isspace(p[l - 1])) p[--l] = 0;
    while (*p && isspace(*p)) ++p, --l;

    memmove(s, p, l + 1);

    // Dealing with escape space at the end of command
    if (s[strlen(s) - 1] == '\\') {
        // Add another space if the last character is a space
        s[strlen(s)]     = ' ';
        s[strlen(s) + 1] = '\0';
    }
    DBG_MSG("%-30s'%s'\n", "[emplace_trim]", s);
}

static inline char *trim(const char *str)
{
    char *s = strdup(str);
    emplace_trim(s);
    return s;
}

// Tokenize string by spaces
static inline int tokenize(char *str)
{
    int i    = 0;
    int size = 0;
    int cnt  = 1;
    // Escape state flag
    bool escape_flag = false;

    if (str != NULL) size = strlen(str);
    // String tokenize
    for (i = 0; i < size; i++) {
        char c  = str[i];
        char lc = (i == 0) ? c : str[i - 1];
        // Skip escape characters
        if (!escape_flag && c == '\\') {
            i++;
            continue;
        }
        // Escape quated string
        if (!escape_flag && isquote(c)) {
            escape_flag = 1;
        } else if (escape_flag && isquote(c)) {
            escape_flag = 0;
        }
        // Set \0 to all spaces which is not escaped
        if (!escape_flag && isspace(c)) {
            str[i] = '\0';
            // Only count the counter when the space does not appear continuously
            if (lc != '\0') cnt++;
        }
    }

    return cnt;
}

// Return pointer to the next token, NULL if it fails or ends
static inline char *next_token(char *ptr, const char *ptr_end)
{
    int i = 0;

    for (; ptr != ptr_end; i++) {
        if (ptr[i] == '\0') break;
    }
    for (; ptr != ptr_end; i++) {
        if (ptr[i] != '\0') break;
    }

    if (ptr == ptr_end) return NULL;
    return &ptr[i];
}

static inline int tokenize_to_argv(const char *str, char **argv)
{
    if (str == NULL || argv == NULL) return 0;
    int   i    = 0;
    int   len  = strlen(str);
    char *ptr  = trim(str);
    char *pch  = NULL;
    int   argc = tokenize(ptr);

    pch = ptr;
    for (i = 0; i < argc; i++) {
        argv[i] = pch;
        DBG_MSG("%-30s%s\n", "[tokenize_to_argv]", argv[i]);
        pch = next_token(pch, ptr + len);
    }
    return argc;
}

static inline char *join_path(char *path, const char *path_later)
{
    if (!endwith(path, "/")) strcat(path, "/");
    strcat(path, path_later);
    // Remove the tailing slash
    if (endwith(path, "/")) path[strlen(path) - 1] = '\0';
    DBG_MSG("%-30s'%s'\n", "[join_path]", path);
    return path;
}

static inline char *locate_binary(const char *bname)
{
    char *sys_path        = NULL;
    char *pch             = NULL;
    char *out_path        = NULL;
    char  full_path[1024] = {};

    if (bname == NULL) return strdup("");
    sys_path = strdup(getenv("PATH"));
    pch      = strtok(sys_path, ":");
    while (pch != NULL) {
        strcpy(full_path, pch);
        join_path(full_path, bname);
        if (access(full_path, X_OK) != -1) { // File exist and is executable
            out_path = strdup(full_path);
            break;
        }
        pch = strtok(NULL, ":");
    }
    free(sys_path);

    DBG_MSG("%-30s'%s'\n", "[locate_binary]", out_path);
    if (out_path == NULL) return strdup(bname);
    return out_path;
}

static inline char *locate_path(const char *bname)
{
    char *sys_path        = NULL;
    char *pch             = NULL;
    char *out_path        = NULL;
    char  full_path[1024] = {};

    if (bname == NULL) return strdup("");
    sys_path = strdup(getenv("PATH"));
    pch      = strtok(sys_path, ":");
    while (pch != NULL) {
        strcpy(full_path, pch);
        join_path(full_path, bname);
        if (access(full_path, X_OK) != -1) { // File exist and is executable
            out_path = strdup(pch);
            break;
        }
        pch = strtok(NULL, ":");
    }
    free(sys_path);

    DBG_MSG("%-30s'%s'\n", "[locate_path]", out_path);
    if (out_path == NULL) return strdup("");
    return out_path;
}

static inline char *get_library_path(const char *message)
{
    char buff[1024] = "";

    if (strstr(message, "=>")) {
        // Format: libc.so.6 => /usr/lib/libc.so.6 (0x0000777777777777)
        // Get the string after "=>" and have it trimmed.
        char *str = trim(strstr(message, "=>") + strlen("=>"));
        if (strstr(str, "not found")) {
            DBG_MSG("%-30s'%s'\n", "[get_library_path]", message);
            free(str);
            return NULL;
        }
        // Tokenize the string will get us only the path
        tokenize(str);
        // Solve the symbolic link
        if (realpath(str, buff) == NULL) {
            // Output the path directly if it fails
            strncpy(buff, str, sizeof(buff));
        }
        free(str);
    } else {
        // Format: linux-vdso.so.1 (0x0000777777777777)
        // Get the string trimmed.
        char *str = trim(message);
        // Tokenize the string will get us only the path
        tokenize(str);
        if (realpath(str, buff) == NULL) {
            // Output the path directly if it fails
            strncpy(buff, str, sizeof(buff));
        }
        free(str);
    }

    DBG_MSG("%-30s'%s'\n", "[get_library_path]", buff);
    return strdup(buff);
}

#endif
