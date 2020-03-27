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
#include <proc.h>

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
int sys_open(userptr_t filename, int flags, mode_t mode, int32_t* retval) {
    
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
    result = vfs_open(filenameCopy, flags, mode, &vnode);
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
    *retval = (int32_t)fdtIndex;
    return 0;

}


/* Create a uio struct for use with VOP_READ and VOP_WRITE */
void uio_uinit(struct iovec *iov, struct uio *u, userptr_t buf, size_t len, off_t offset, enum uio_rw rw){
    iov->iov_ubase  = buf;
    iov->iov_len    = len;

    u->uio_iov      = iov;
    u->uio_iovcnt   = 1;
    u->uio_offset   = offset;
    u->uio_resid    = len;
    u->uio_segflg   = UIO_USERSPACE;
    u->uio_rw       = rw;
    u->uio_space    = proc_getas();
}

/* Read data from file */
ssize_t sys_read(int fd, void *buf, size_t buflen, int32_t* retval) {

    /* First we need to check that the fd is a valid file handle */
    if (fd < 0 || fd >= OPEN_MAX) {
        return EBADF;
    }

    /* Then we need to match it to an entry in the open file table, and check that the entry has been opened*/
    int oftIndex = curproc->p_fdt[fd];
    if (oftIndex == -1) {
        return EBADF;
    }

    /* Set up the kernel buffer */
    void *safeBuff[buflen];


    /* Next we acquire the lock for the open file table, as we are about to modify the file pointer */
    lock_acquire(oft->oftLock);

    /* Then we load the current offset (file pointer) */

    int offset = oft->oftArray[oftIndex]->fp;

    /* Next create the uio variable that needs to be passed to VOP_READ*/
    struct iovec *iov = kmalloc(sizeof(struct iovec));
    struct uio *u = kmalloc(sizeof(struct uio));
    uio_uinit(iov, u, (userptr_t)safeBuff, buflen, offset, UIO_READ);

    /* Call the VOP_READ macro*/
    int result = VOP_READ(oft->oftArray[oftIndex]->vnode, u);
    if (result) {
        lock_release(oft->oftLock);
        return result;
    }

    /* Update the offset (file pointer) */
    oft->oftArray[oftIndex]->fp = u->uio_offset;

    /* Release the lock */
    lock_release(oft->oftLock);

    /* Free malloced memory */
    kfree(u);
    kfree(iov);

    /* Copy data to the user's pointer */
    result = copyout(safeBuff, (userptr_t)buf, buflen);
    if (result) {
        return result;
    }


    /* Return the amount read*/
    *retval = u->uio_resid;
    return 0;
}

/* Write data to file */
ssize_t sys_write(int fd, void *buf, size_t nbytes, int32_t* retval) {

    /* First we need to check that the fd is a valid file handle */
    if (fd < 0 || fd >= OPEN_MAX) {
        return EBADF;
    }

    /* Then we need to match it to an entry in the open file table, and check that the entry has been opened*/
    int oftIndex = curproc->p_fdt[fd];
    if (oftIndex == -1) {
        return EBADF;
    }

    /* Copy in the user's pointer */
    void *safeBuff[nbytes];
    int result = copyin((userptr_t)buf, safeBuff, nbytes);
    if (result) {
        return result;
    }


    /* Next we acquire the lock for the open file table, as we are about to modify the file pointer */
    lock_acquire(oft->oftLock);

    /* Then we load the current offset (file pointer) */

    int offset = oft->oftArray[oftIndex]->fp;

    /* Next create the uio variable that needs to be passed to VOP_READ*/
    struct iovec *iov = kmalloc(sizeof(struct iovec));
    struct uio *u = kmalloc(sizeof(struct uio));
    uio_uinit(iov, u, (userptr_t)safeBuff, nbytes, offset, UIO_WRITE);

    /* Call the VOP_READ macro*/
    result = VOP_WRITE(oft->oftArray[oftIndex]->vnode, u);
    if (result) {
        lock_release(oft->oftLock);
        return result;
    }

    /* Update the offset (file pointer) */
    oft->oftArray[oftIndex]->fp = u->uio_offset;

    /* Release the lock */
    lock_release(oft->oftLock);

    /* Free malloced memory */
    kfree(u);
    kfree(iov);

    /* Return the amount read*/
    *retval = u->uio_resid;
    return 0;
}

/* Change current position in file */
off_t sys_lseek(int fd, off_t pos, int whence, int64_t* retval) {

    /* First we need to check that the fd is a valid file handle */
    if (fd < 0 || fd >= OPEN_MAX) {
        return EBADF;
    }

    /* Then we need to match it to an entry in the open file table, and check that the entry has been opened*/
    int oftIndex = curproc->p_fdt[fd];
    if (oftIndex == -1) {
        return EBADF;
    }


    /* Next we acquire the lock for the open file table, as we are about to modify the file pointer */
    lock_acquire(oft->oftLock);

    /* Check that the given file is seekable */
    bool isSeekable = VOP_ISSEEKABLE(oft->oftArray[oftIndex]->vnode);
    if (!isSeekable){
        lock_release(oft->oftLock);
        return ESPIPE;
    }

    off_t newFP;

    /* Identify the whence option */
    if (whence == SEEK_SET){
        /* Set file pointer to the position relative to the start of the file */
        newFP = pos;
    }
    else if (whence == SEEK_CUR){
        /* Set file pointer to the position relative to the current position */
        newFP = oft->oftArray[oftIndex]->fp;
    }
    else if (whence == SEEK_END){
        /* Set file pointer to the position relative to the end of the file */

        /* Get the size of the current file */
        struct stat *fStat = kmalloc(sizeof(struct stat));
        int result = VOP_STAT(oft->oftArray[oftIndex]->vnode, fStat);
        if (result) {
            lock_release(oft->oftLock);
            return result;
        }

        /* Calculate the new file pointer */
        newFP = fStat->st_size + oft->oftArray[oftIndex]->fp;

        /* Free malloced memory */
        kfree(fStat);
    }
    else {
        lock_release(oft->oftLock);
        return EINVAL;
    }

    /* Check that the new file pointer is valid*/
    if (newFP < 0){
        lock_release(oft->oftLock);
        return EINVAL;
    }

    /* Update the file pointer */
    oft->oftArray[oftIndex]->fp = newFP;

    /* Release the lock */
    lock_release(oft->oftLock);

    /* return the new file pointer */
    *retval = newFP;
    return 0;
}

/* Close file */
int sys_close(int fd, int32_t* retval) {

    /* First we want to match the fd to the entry in the open file table */
    int oftIndex = curproc->p_fdt[fd];

    /* Then we check that the fd is a valid file handle */
    if (fd < 0 || fd >= OPEN_MAX || oftIndex == -1) {
        return EBADF;
    }

    /* Next we acquire the lock for the open file table,
    as we are about to delete an entry */
    lock_acquire(oft->oftLock);

    /* We also check to see if the reference to the open file table is valid */
    if (oft->oftArray[oftIndex] == NULL) {
        lock_release(oft->oftLock);
        return EBADF;
    }

    /* Here we declare this entry in the file descriptor table as closed */
    curproc->p_fdt[fd] = -1;

    /* If there are other references to this vnode,
    we simply decrement the reference count */
    if (oft->oftArray[oftIndex]->referenceCount > 1) {
        oft->oftArray[oftIndex]->referenceCount--;
    /* Else if this is the only reference, we want to close the vnode */
    } else {
        vfs_close(oft->oftArray[oftIndex]->vnode);
        kfree(oft->oftArray[oftIndex]);
        oft->oftArray[oftIndex] = NULL;
    }

    /* After removing this entry, we can release the lock */
    lock_release(oft->oftLock);

    *retval = 0;
    return 0;
}

/* Clone file handles */
int sys_dup2(int oldfd, int newfd, int32_t* retval) {
    
    /* First we want to match the oldfd to the entry in the open file table */
    int oftIndex = curproc->p_fdt[oldfd];

    /* Then we check that the oldfd is a valid file handle */
    if (oldfd < 0 || oldfd >= OPEN_MAX || oftIndex == -1) {
        return EBADF;
    }

    /* We also want to check that the value of newfd can be
    a valid file handle */
    if (newfd < 0 || newfd >= OPEN_MAX) {
        return EBADF;
    }

    /* Next we acquire the lock for the open file table,
    as we are about to add an entry */
    lock_acquire(oft->oftLock);

    /* We also check to see if the reference to the open file table is valid */
    if (oft->oftArray[oftIndex] == NULL) {
        lock_release(oft->oftLock);
        return EBADF;
    }

    /* Cloning a file handle onto itself has no effect,
    so simply return newfd */
    if (oldfd == newfd) {
        lock_release(oft->oftLock);
        *retval = newfd;
        return 0;
    }

    /* If newfd names an already open file, that file is closed */
    if (curproc->p_fdt[newfd] != -1) {

        /* We close the file by using our implemented sys_close */
        int result = sys_close(newfd, retval);
        if (result) {
            lock_release(oft->oftLock);
            return result;
        }
    }

    /* We then clone the file handle oldfd onto the file handle newfd */
    curproc->p_fdt[newfd] = curproc->p_fdt[oldfd];

    /* For the entry in the open file table, we want to increment
    the reference count, as two fds are now pointing to this vnode */
    oft->oftArray[oftIndex]->referenceCount++;

    /* We can now release the lock after cloning the file handle */
    lock_release(oft->oftLock);

    /* We simply return newfd */
    *retval = newfd;
    return 0;

}

