/* SPDX-License-Identifier: BSD-2-Clause */

#include <tilck/common/basic_defs.h>
#include <tilck/common/string_util.h>

#include <tilck/kernel/fs/vfs.h>
#include <tilck/kernel/kmalloc.h>
#include <tilck/kernel/errno.h>
#include <tilck/kernel/process.h>
#include <tilck/kernel/user.h>

#include <dirent.h> // system header

#include "../fs_int.h"
#include "vfs_locking.c.h"
#include "vfs_resolve.c.h"
#include "vfs_stat.c.h"
#include "vfs_op_ready.c.h"

static u32 next_device_id;

/*
 * ----------------------------------------------------
 * Main VFS functions
 * ----------------------------------------------------
 */

int vfs_open(const char *path, fs_handle *out, int flags, mode_t mode)
{
   const char *fs_path;
   filesystem *fs;
   vfs_path p;
   int rc;

   NO_TEST_ASSERT(is_preemption_enabled());
   ASSERT(path != NULL);
   ASSERT(*path == '/'); /* VFS works only with absolute paths */

   if (flags & O_ASYNC)
      return -EINVAL; /* TODO: Tilck does not support ASYNC I/O yet */

   if ((flags & O_TMPFILE) == O_TMPFILE)
      return -EOPNOTSUPP; /* TODO: Tilck does not support O_TMPFILE yet */

   if (!(fs = get_retained_fs_at(path, &fs_path)))
      return -ENOENT;

   /* See the comment in vfs.h about the "fs-lock" funcs */
   vfs_fs_exlock(fs);
   {
      rc = vfs_resolve(fs, fs_path, &p);

      if (!rc)
         rc = fs->fsops->open(&p, out, flags, mode);
   }
   vfs_fs_exunlock(fs);

   if (rc == 0) {

      /* open() succeeded, the FS is already retained */
      ((fs_handle_base *) *out)->fl_flags = flags;

      if (flags & O_CLOEXEC)
         ((fs_handle_base *) *out)->fd_flags |= FD_CLOEXEC;

   } else {
      /* open() failed, we need to release the FS */
      release_obj(fs);
   }

   return rc;
}

void vfs_close(fs_handle h)
{
   /*
    * TODO: consider forcing also vfs_close() to be run always with preemption
    * enabled. Reason: when one day when actual I/O devices will be supported,
    * close() might need in some cases to do some I/O.
    *
    * What prevents vfs_close() to run with preemption enabled is the function
    * terminate_process() which requires disabled preemption, because of its
    * (primitive) sync with signal handling.
    */
   ASSERT(h != NULL);

   fs_handle_base *hb = (fs_handle_base *) h;
   filesystem *fs = hb->fs;

#ifndef UNIT_TEST_ENVIRONMENT
   process_info *pi = get_curr_task()->pi;
   remove_all_mappings_of_handle(pi, h);
#endif

   fs->fsops->close(h);
   release_obj(fs);

   /* while a filesystem is mounted, the minimum ref-count it can have is 1 */
   ASSERT(get_ref_count(fs) > 0);
}

int vfs_dup(fs_handle h, fs_handle *dup_h)
{
   ASSERT(h != NULL);

   fs_handle_base *hb = (fs_handle_base *) h;
   int rc;

   if (!hb)
      return -EBADF;

   if ((rc = hb->fs->fsops->dup(h, dup_h)))
      return rc;

   /* The new file descriptor does NOT share old file descriptor's fd_flags */
   ((fs_handle_base*) *dup_h)->fd_flags = 0;

   retain_obj(hb->fs);
   ASSERT(*dup_h != NULL);
   return 0;
}

ssize_t vfs_read(fs_handle h, void *buf, size_t buf_size)
{
   NO_TEST_ASSERT(is_preemption_enabled());
   ASSERT(h != NULL);

   fs_handle_base *hb = (fs_handle_base *) h;
   ssize_t ret;

   if (!hb->fops->read)
      return -EBADF;

   if ((hb->fl_flags & O_WRONLY) && !(hb->fl_flags & O_RDWR))
      return -EBADF; /* file not opened for reading */

   vfs_shlock(h);
   {
      ret = hb->fops->read(h, buf, buf_size);
   }
   vfs_shunlock(h);
   return ret;
}

ssize_t vfs_write(fs_handle h, void *buf, size_t buf_size)
{
   NO_TEST_ASSERT(is_preemption_enabled());
   ASSERT(h != NULL);

   fs_handle_base *hb = (fs_handle_base *) h;
   ssize_t ret;

   if (!hb->fops->write)
      return -EBADF;

   if (!(hb->fl_flags & (O_WRONLY | O_RDWR)))
      return -EBADF; /* file not opened for writing */

   vfs_exlock(h);
   {
      ret = hb->fops->write(h, buf, buf_size);
   }
   vfs_exunlock(h);
   return ret;
}

off_t vfs_seek(fs_handle h, s64 off, int whence)
{
   NO_TEST_ASSERT(is_preemption_enabled());
   ASSERT(h != NULL);
   off_t ret;

   if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END)
      return -EINVAL; /* Tilck does NOT support SEEK_DATA and SEEK_HOLE */

   fs_handle_base *hb = (fs_handle_base *) h;

   if (!hb->fops->seek)
      return -ESPIPE;

   vfs_shlock(h);
   {
      // NOTE: this won't really work for big offsets in case off_t is 32-bit.
      ret = hb->fops->seek(h, (off_t) off, whence);
   }
   vfs_shunlock(h);
   return ret;
}

int vfs_ioctl(fs_handle h, uptr request, void *argp)
{
   NO_TEST_ASSERT(is_preemption_enabled());
   ASSERT(h != NULL);

   fs_handle_base *hb = (fs_handle_base *) h;
   int ret;

   if (!hb->fops->ioctl)
      return -ENOTTY; // Yes, ENOTTY *IS* the right error. See the man page.

   vfs_exlock(h);
   {
      ret = hb->fops->ioctl(h, request, argp);
   }
   vfs_exunlock(h);
   return ret;
}

int vfs_fcntl(fs_handle h, int cmd, int arg)
{
   NO_TEST_ASSERT(is_preemption_enabled());
   fs_handle_base *hb = (fs_handle_base *) h;
   int ret;

   if (!hb->fops->fcntl)
      return -EINVAL;

   vfs_exlock(h);
   {
      ret = hb->fops->fcntl(h, cmd, arg);
   }
   vfs_exunlock(h);
   return ret;
}

int vfs_mkdir(const char *path, mode_t mode)
{
   const char *fs_path;
   filesystem *fs;
   vfs_path p;
   int rc;

   NO_TEST_ASSERT(is_preemption_enabled());
   ASSERT(path != NULL);
   ASSERT(*path == '/'); /* VFS works only with absolute paths */

   if (!(fs = get_retained_fs_at(path, &fs_path)))
      return -ENOENT;

   if (!(fs->flags & VFS_FS_RW))
      return -EROFS;

   if (!fs->fsops->mkdir)
      return -EPERM;

   /* See the comment in vfs.h about the "fs-lock" funcs */
   vfs_fs_exlock(fs);
   {
      rc = vfs_resolve(fs, fs_path, &p);

      if (!rc)
         rc = fs->fsops->mkdir(&p, mode);
   }
   vfs_fs_exunlock(fs);
   release_obj(fs);     /* it was retained by get_retained_fs_at() */
   return rc;
}

int vfs_rmdir(const char *path)
{
   const char *fs_path;
   filesystem *fs;
   vfs_path p;
   int rc;

   NO_TEST_ASSERT(is_preemption_enabled());
   ASSERT(path != NULL);
   ASSERT(*path == '/'); /* VFS works only with absolute paths */

   if (!(fs = get_retained_fs_at(path, &fs_path)))
      return -ENOENT;

   if (!(fs->flags & VFS_FS_RW))
      return -EROFS;

   if (!fs->fsops->rmdir)
      return -EPERM;

   /* See the comment in vfs.h about the "fs-lock" funcs */
   vfs_fs_exlock(fs);
   {
      rc = vfs_resolve(fs, fs_path, &p);

      if (!rc)
         rc = fs->fsops->rmdir(&p);
   }
   vfs_fs_exunlock(fs);
   release_obj(fs);     /* it was retained by get_retained_fs_at() */
   return rc;
}

int vfs_unlink(const char *path)
{
   const char *fs_path;
   filesystem *fs;
   vfs_path p;
   int rc;

   NO_TEST_ASSERT(is_preemption_enabled());
   ASSERT(path != NULL);
   ASSERT(*path == '/'); /* VFS works only with absolute paths */

   if (!(fs = get_retained_fs_at(path, &fs_path)))
      return -ENOENT;

   if (!(fs->flags & VFS_FS_RW))
      return -EROFS;

   if (!fs->fsops->unlink)
      return -EROFS;

   /* See the comment in vfs.h about the "fs-lock" funcs */
   vfs_fs_exlock(fs);
   {
      rc = vfs_resolve(fs, fs_path, &p);

      if (!rc)
         rc = fs->fsops->unlink(&p);
   }
   vfs_fs_exunlock(fs);
   release_obj(fs);     /* it was retained by get_retained_fs_at() */
   return rc;
}

u32 vfs_get_new_device_id(void)
{
   return next_device_id++;
}