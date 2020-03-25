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

/* Open file table array */
typedef struct openFileTable {
    struct lock *oftLock; /* A simple lock for the open file table to provide mutual exclusion */
    oftEntry *oftArray[OPEN_MAX]; /* Each entry in array is of type oftEntry */
} openFileTable;

/* Entry in the open file table array */
typedef struct oftEntry {
    struct vnode *vnode; /* Pointer to the vnode for the file */
    off_t fp; /* Filepointer provides the offset for reading and writing */
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

#endif /* _FILE_H_ */
