#ifndef __UTIL_H__
#define __UTIL_H__
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#define STRCMP(a, R, b) (0 R strcmp(a, b))
#if  (__GNUC__ >= 3)
#define restrict __restrict
#else
#define restrict /**/
#endif

#ifndef u8
#define u8 unsigned char
#endif

#ifndef u16
#define u16 unsigned short
#endif

#ifndef u32
#define u32 unsigned int
#endif

#ifndef u64
#define u64 unsigned long long
#endif

#define NOT_MATCH	-1
#ifndef ZEROMEMORY
#define ZEROMEMORY(x, y) memset((void*)x, 0, y)
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif

char lwr(char a);
int lwrcmp(const char *as, const char *bs, int n);
char i2h(int i);
int b64cpy(char *restrict dst, const char *restrict src, int n, int fss);

#endif
