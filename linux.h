#ifndef __LINUX__
#define __LINUX__
#define exec execv
#undef exit
#undef printf
#define printf dprintf
int exit(int) __attribute__((noreturn));
#include <errno.h>
#define exit() exit(0)
#undef O_CREATE
#define O_CREATE 0100 /* Fix file creation on linux */
#endif

