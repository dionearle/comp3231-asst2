/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>


/*
 * Put your function declarations and data types here ...
 */

/* Per-process file descriptor array */
typedef struct fileDescriptorTable {
    int fdtArray[OPEN_MAX]; /* Array contains pointers to open file table entries */
} fileDescriptorTable;

/* Open file table array */
typedef struct openFileTable {
    struct lock *openFileTableLock; /* A simple lock for the open file table to provide mutual exclusion */
    oftEntry *oftArray[OPEN_MAX]; /* Each entry in array is of type oftEntry */
} openFileTable;

/* Entry in the open file table array */
typedef struct oftEntry {
    struct vnode *vnode; /* Pointer to the vnode for the file */
    off_t offset; /* The file's offset for reading and writing */
    int flags; /* Outlines the permissions of the file */
    int referenceCount; /* Tracks how many references of the file exist */
} oftEntry;

/* Global open file table */
openFileTable *oft;

/* Attach stdout and stderr to the console device */
int consoleDeviceSetup(void);

/* Initialise the global open file table array */
int openFileTableSetup(void);

/* Initialise the file descriptor array for the current process */
int fileDescriptorTableSetup(void);

/* Open a file */
int sys_open(const char *filename, int flags, mode_t mode);

/* Read data from file */
ssize_t sys_read(int fd, void *buf, size_t buflen);

/* Write data to file */
ssize_t sys_write(int fd, const void *buf, size_t nbytes);

/* Change current position in file */
off_t sys_lseek(int fd, off_t pos, int whence);

/* Close file */
int sys_close(int fd);

/* Clone file handles */
int sys_dup2(int oldfd, int newfd);

#endif /* _FILE_H_ */
