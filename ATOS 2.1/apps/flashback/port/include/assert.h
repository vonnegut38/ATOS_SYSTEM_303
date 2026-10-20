#ifndef ATOS_ASSERT_H
#define ATOS_ASSERT_H
#ifdef __cplusplus
extern "C"
#endif
void atos_assert_fail(const char *expr,const char *file,int line) __attribute__((noreturn));
#undef assert
#ifdef NDEBUG
#define assert(e) ((void)0)
#else
#define assert(e) ((e)?(void)0:atos_assert_fail(#e,__FILE__,__LINE__))
#endif
#endif
