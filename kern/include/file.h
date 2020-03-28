/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>
#include <uio.h>

/*
 * Put your function declarations and data types here ...
 */

/* Entry in the open file table array */
typedef struct _oftEntry {
    struct vnode *vnode; /* Pointer to the vnode for the file */
    off_t fp; /* Filepointer provides the offset for reading and writing */
    int flags; /* Outlines the permissions of the file */
    int referenceCount; /* Tracks how many references of the file exist */
} oftEntry;

/* Open file table array */
typedef struct _openFileTable {
    struct lock *oftLock; /* A simple lock for the open file table to provide mutual exclusion */
    oftEntry *oftArray[OPEN_MAX]; /* Each entry in array is of type oftEntry */
} openFileTable;

/* Global open file table */
openFileTable *oft;

/* Initialise the global open file table array */
int openFileTableSetup(void);

/* Initialise the file descriptor array for the current process */
int fileDescriptorTableSetup(void);

/* Attach stdout and stderr to the console device */
int consoleDeviceSetup(void);

// /* Attach stdout and stderr to the console device */
// void uio_uinit(struct iovec *iov, struct uio *u, userptr_t buf, size_t len, off_t offset, enum uio_rw rw);



#endif /* _FILE_H_ */
