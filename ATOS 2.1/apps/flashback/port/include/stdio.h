/* Minimal stdio for the ATOS Flashback port (port/libc.c). */
#ifndef ATOS_STDIO_H
#define ATOS_STDIO_H
#include <stddef.h>
#include <stdarg.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct atos_file FILE;
extern FILE *stdin,*stdout,*stderr;
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define BUFSIZ 1024
FILE *fopen(const char *path,const char *mode);
int fclose(FILE *f);
size_t fread(void *ptr,size_t size,size_t n,FILE *f);
size_t fwrite(const void *ptr,size_t size,size_t n,FILE *f);
int fseek(FILE *f,long off,int whence);
long ftell(FILE *f);
int feof(FILE *f);
int fflush(FILE *f);
char *fgets(char *s,int n,FILE *f);
int fgetc(FILE *f);
int fputc(int c,FILE *f);
int fputs(const char *s,FILE *f);
int puts(const char *s);
int putchar(int c);
int printf(const char *fmt,...) __attribute__((format(printf,1,2)));
int fprintf(FILE *f,const char *fmt,...) __attribute__((format(printf,2,3)));
int sprintf(char *s,const char *fmt,...) __attribute__((format(printf,2,3)));
int snprintf(char *s,size_t n,const char *fmt,...) __attribute__((format(printf,3,4)));
int vprintf(const char *fmt,va_list ap);
int vfprintf(FILE *f,const char *fmt,va_list ap);
int vsprintf(char *s,const char *fmt,va_list ap);
int vsnprintf(char *s,size_t n,const char *fmt,va_list ap);
int remove(const char *path);
#ifdef __cplusplus
}
#endif
#endif
