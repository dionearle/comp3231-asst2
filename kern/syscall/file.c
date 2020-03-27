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

/* Initialise the global open file table array */
int openFileTableSetup(void) {

    /* We assign the memory for the table on the heap */
    oft = kmalloc(sizeof(openFileTable));

    /* If the malloc failed, we return an error signalling we are out of memory */
    if (oft == NULL) {
        return ENOMEM;
    }

    /* Now the open file table is created, we first create the lock for it */
    oft->oftLock = lock_create("oftLock");

    /* If this failed, we return an error signalling we are out of memory*/
    if (oft->oftLock == NULL) {
        return ENOMEM;
    }

    /* And finally we initialise the array to be empty */
    int i = 0;
    while (i < OPEN_MAX) {
        oft->oftArray[i] = NULL;
        i++;
    }

    return 0;
}

/* Initialise the file descriptor array for the current process */
int fileDescriptorTableSetup(void) {

    /* First we assign the memory for the file descriptor table on the heap */
    curproc->p_fdt = kmalloc(sizeof(int) * OPEN_MAX);

    /* If this failed, we return an error signalling we are out of memory */
    if (curproc->p_fdt == NULL) {
        return ENOMEM;
    }

    /* Then we simply declare all entries in this array as closed */
    int i = 0;
    while (i < OPEN_MAX) {
        curproc->p_fdt[i] = -1;
        i++;
    }

    return 0;
}

/* Attach stdout and stderr to the console device */
int consoleDeviceSetup(void) {

    int i = 1;
    while (i <= 2) {

        /* First we initialise the variables needed for the 
        upcoming vfs_open call */
        char c[] = "con:";
        int f = O_WRONLY;
        int m = 0;
        struct vnode *v;

        /* Now we attach stdout to the console device */
        int result = vfs_open(c, f, m, &v);
        if (result) {
            return result;
        }

        /* Next we acquire the lock for the open file table,
        as we are about to add an entry */
        lock_acquire(oft->oftLock);

        /* We allocate memory on the heap for the entry to be
        added to the open file table at index 1 for stdout
        and index 2 for stderr */
        oft->oftArray[i] = kmalloc(sizeof(oftEntry));

        /* If this failed, we return an error signalling we are out of memory */
        if (oft->oftArray == NULL) {
            vfs_close(v);
            lock_release(oft->oftLock);
            return ENOMEM;
        }

        /* We assign the processes' file descriptor to the index
        in the global open file table */
        curproc->p_fdt[i] = i;

        /* Then we assign all of the information needed 
        in the open file table entry */
        oft->oftArray[i]->vnode = v;
        oft->oftArray[i]->fp = 0;
        oft->oftArray[i]->flags = f;
        oft->oftArray[i]->referenceCount = 1;

        /* Once we are done adding this entry, we can release the lock */
        lock_release(oft->oftLock);

        i++;
    }

    return 0;
}

/* Open a file */
int sys_open(userptr_t filename, int flags, mode_t mode) {
    
    /* First we check that filename is a valid pointer */
    if (filename == NULL) {
        return EFAULT;
    }

    /* Then we obtain a copy of the filename inside the buffer
    using copyinstr */
    char filenameCopy[PATH_MAX];
    int result = copyinstr(filename, filenameCopy, PATH_MAX, NULL);
    if (result) {
        return result;
    }

    /* Once we have a copy of the filename and a vnode object,
    we can call vfs_open */
    struct vnode *vnode;
    int result = vfs_open(filenameCopy, flags, mode, &vnode);
    if (result) {
        return result;
    }

    /* Next we acquire the lock for the open file table,
    as we are about to add an entry */
    lock_acquire(oft->oftLock);

    /* To determine where to add this new entry to the open file table,
    we search for the first available index in the array */
    int oftIndex = -1;
    int i = 0;
    while (i < OPEN_MAX) {

        if (oft->oftArray[i] == NULL) {
            oftIndex = i;
            break;
        }

        i++;
    }

    /* If we couldn't find a free entry in the open file table,
    we return an error stating it is full */
    if (!oftIndex) {
        vfs_close(vnode);
        lock_release(oft->oftLock);
        return ENFILE;
    }

    /* We also need to find the first available free entry
    in the processes' file descriptor table */
    int fdtIndex = -1;
    int j = 0;
    while (j < OPEN_MAX) {
        
        if (curproc->p_fdt[j] == -1) {
            fdtIndex = j;
            break;
        }

        j++;
    }

    /* If we couldn't find a free entry in the file descriptor table,
    we return an error stating it is full */
    if (!fdtIndex) {
        vfs_close(vnode);
        lock_release(oft->oftLock);
        return EMFILE;
    }

    /* Given there is a free entry in the open file table,
    we allocate memory on the heap for it */
    oft->oftArray[oftIndex] = kmalloc(sizeof(oftEntry));

    /* If this failed, we return an error signalling we are out of memory */
    if (oft->oftArray == NULL) {
        vfs_close(vnode);
        lock_release(oft->oftLock);
        return ENOMEM;
    }

    /* We assign the processes' file descriptor to the index
    in the global open file table */
    curproc->p_fdt[fdtIndex] = oftIndex;

    /* Then we assign all of the information needed 
    in the open file table entry */
    oft->oftArray[oftIndex]->vnode = vnode;
    oft->oftArray[oftIndex]->fp = 0;
    oft->oftArray[oftIndex]->flags = flags;
    oft->oftArray[oftIndex]->referenceCount = 1;

    /* Once we are done adding this entry, we can release the lock */
    lock_release(oft->oftLock);

    /* We return the file handle */
    return fdtIndex;

}

/* Read data from file */
ssize_t sys_read(int fd, void *buf, size_t buflen) {
    // TODO
}

/* Write data to file */
ssize_t sys_write(int fd, void *buf, size_t nbytes) {
    // TODO
}

/* Change current position in file */
off_t sys_lseek(int fd, off_t pos, int whence) {
    // TODO
}

/* Close file */
int sys_close(int fd) {
    // TODO
}

/* Clone file handles */
int sys_dup2(int oldfd, int newfd) {
    // TODO
}

