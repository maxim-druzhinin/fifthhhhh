#include "kernel/defMyStructs.h"
#include "kernel/budMemInfo.h"

struct stat;

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(const char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
int dummy(void);
int ps_list(int limit, int* pids);
int ps_info(int pid, struct proccess_info *pinfo);
int ps_pt0(int pid, uint64* tableAddr);
int ps_pt1(int pid, void* addr, uint64* tableAddr);
int ps_pt2(int pid, void* addr, uint64* tableAddr);
int ps_copy(int pid, void* addr, int size, void* data);
int ps_sleep_info(int pid, struct write_info *info);
int clone(void);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void fprintf(int, const char*, ...);
void printf(const char*, ...);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);
void mem_dump(void);

