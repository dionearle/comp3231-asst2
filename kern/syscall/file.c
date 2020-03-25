#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>

/*
 * Add your file-related functions here ...
 */

/* Attach stdout and stderr to the console device */
int consoleDeviceSetup(void);

/* Initialise the global open file table array */
int openFileTableSetup(void);

/* Initialise the file descriptor array for the current process */
int fileDescriptorTableSetup(void);

/* Open a file */
int sys_open(userptr_t filename, int flags, mode_t mode);

/* Read data from file */
ssize_t sys_read(int fd, void *buf, size_t buflen);

/* Write data to file */
ssize_t sys_write(int fd, void *buf, size_t nbytes);

/* Change current position in file */
off_t sys_lseek(int fd, off_t pos, int whence);

/* Close file */
int sys_close(int fd);

/* Clone file handles */
int sys_dup2(int oldfd, int newfd);

