struct stat;

// #define unsigned int  unsigned int
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
int waitx(int*, int* /*wtime*/, int* /*rtime*/);
// user.h
int sigalarm(int ticks, void (*handler)());
int sigreturn();
int getSysCount(int mask, int pid);
// user.h
int settickets(int count);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void fprintf(int, const char*, ...);
void printf(const char*, ...);
char* gets(char*, int max);
unsigned int  strlen(const char*);
void* memset(void*, int, unsigned int );
void* malloc(unsigned int );
void free(void*);
int atoi(const char*);
int memcmp(const void *, const void *, unsigned int );
void *memcpy(void *, const void *, unsigned int );
