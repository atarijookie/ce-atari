#!/usr/bin/env python

"""
Based on Passthrough example by Stavros Korokithakis
https://www.stavros.io/posts/python-fuse-filesystem/

Mount linux filesystem to some directory, the filenames will be translated
to 8.3 file naming convention on the fly. 

Jookie (Miro Nohaj) - 2022-01-03
"""

from __future__ import with_statement

import os
import sys
import errno
import logging
import re
from logging.handlers import RotatingFileHandler

from fuse import FUSE, FuseOSError, Operations, fuse_get_context


class Fuse83(Operations):
    def __init__(self, root):
        self.root = root

        self.long_to_short = {}             # translate long_filename.extension to SHORT.EXT
        self.long_to_short_exts = {}        # translate extension to EXT
        
        log_formatter = logging.Formatter('%(asctime)s %(levelname)s %(funcName)s(%(lineno)d) %(message)s')

        my_handler = RotatingFileHandler("/var/log/fuse83.log", mode='a', maxBytes=1024*1024, backupCount=1, encoding=None, delay=0)
        my_handler.setFormatter(log_formatter)
        my_handler.setLevel(logging.DEBUG)

        self.log = logging.getLogger('root')
        self.log.setLevel(logging.DEBUG)
        self.log.addHandler(my_handler)

        self.log.debug("starting with root: {}".format(root))

    # filename shortening
    # ===================

    def got_short_extension(self, short_ext):
        """ return if we already have this short extension assigned to some long extension """
        return any([True for k, v in self.long_to_short_exts.items() if v == short_ext])

    def replace_not_allowed_chars(self, in_string):
        res = re.sub("[^a-zA-Z0-9!#$%&'()~^@-_{}]", '_', in_string)
        return res

    def get_short_extension(self, long_ext):
        if long_ext in self.long_to_short_exts:         # got short version for this longer extension already? use it
            ext = self.long_to_short_exts.get(long_ext)
            return ext

        # don't have short version yet, check if short already
        ext = self.replace_not_allowed_chars(long_ext)  # replace not allowed chars
        ext = ext.upper()                               # to uppercase

        if len(ext) <= 3:                               # extension short? use it
            self.long_to_short_exts[long_ext] = ext     # store for reusing later
            return ext

        # so the ext is long
        ext_cut = ext[0:3]                          # try just to cut ext to length of 3
        got_cut = self.got_short_extension(ext_cut)

        if not got_cut:                             # don't have long ext, don't have cut ext? can use cut ext
            self.long_to_short_exts[long_ext] = ext_cut
            return ext_cut

        # this cut ext already exists?
        ext_new = None

        for i in range(1, 999):                 # find which short version of extension can be used
            ext_num = str(i)                    # number to string (e.g. 85 -> "85")
            ext_num_len = len(ext_num)          # length of ext number string (e.g. 2 for "85")

            ext_part_len = 3 - ext_num_len      # length of original ext that can be used together with ext number string to have length 3 chars
            ext_part = ext[0:ext_part_len]      # part of original extension

            ext_new = ext_part + ext_num        # new extension (e.g. "M85")

            if not self.got_short_extension(ext_new):       # don't have this new extension, can use it
                break

        # store new extension and return value
        # (note: can cause collision on 999 same shorted extensions)
        self.long_to_short_exts[long_ext] = ext_new
        return ext_new


    def get_short_filename(self, long_filename_with_ext, long_filename, ext_short):
        """ use this function to convert long_filename_with_ext to short_filename_with_ext """

        # got short version for this longer extension already? use it
        if long_filename_with_ext in self.long_to_short:
            short_filename_with_ext = self.long_to_short.get(long_filename_with_ext)
            return short_filename_with_ext

        filename = long_filename

        # if empty, fill with '_' - this shouldn't happen
        if not long_filename and not ext_short:
            filename = "________"

        # fix bad characters
        filename = self.replace_not_allowed_chars(filename)
        filename = filename.upper()                 # convert to uppercase

        # filename is short enough? use it
        if len(filename) <= 8:
            if ext_short is not None:                                               # if got extension, use it
                short_filename_with_ext = filename + "." + ext_short                # construct filename.ext
            else:                                                                   # no extension? just filename then
                short_filename_with_ext = filename

            self.long_to_short[long_filename_with_ext] = short_filename_with_ext    # store for reusing later
            return short_filename_with_ext

        # try to find which short filename can be used
        short_filename_with_ext = None

        for i in range(1, 9999):                            # find which short version can be used
            filename_num = str(i)                           # number to string (e.g. 85 -> "85")
            filename_num_len = len(filename_num)            # length of number string (e.g. 2 for "85")

            filename_part_len = 8 - filename_num_len        # length of original that can be used together with number string to have length 8 chars
            filename_part = filename[0:filename_part_len]   # part of original filename

            filename_new = filename_part + filename_num     # new filename (e.g. "LongFi85")

            if ext_short is not None:                       # if got extension, use it
                short_filename_with_ext = filename_new + "." + ext_short
            else:                                           # no extension? just filename then
                short_filename_with_ext = filename_new

            # check if this value is already in the dict
            got_short_filename_with_ext = any([True for k, v in self.long_to_short.items() if v == short_filename_with_ext])

            if not got_short_filename_with_ext:             # don't have this value used for other filename? good
                break

        # store to dict, return value
        self.long_to_short[long_filename_with_ext] = short_filename_with_ext
        return short_filename_with_ext


    def get_short_filename_with_ext(self, long_filename_with_ext):
        """ convert long filename with extension to short filename with short extension"""

        # don't convert . and .. diritems
        if long_filename_with_ext in ['.', '..']:
            return long_filename_with_ext

        # find out if we do have this long file name already, if so then use it
        if long_filename_with_ext in self.long_to_short:
            return self.long_to_short.get(long_filename_with_ext)

        # don't have this long_filename_with_ext in our dict yet
        filename_long, ext_long = os.path.splitext(long_filename_with_ext)

        ext_short = None

        # if extension is present, convert it to short one
        if ext_long:
            ext = ext_long[1:]              # remove '.' from start of the ext
            ext_short = self.get_short_extension(ext)

        short_filename_with_ext = self.get_short_filename(long_filename_with_ext, filename_long, ext_short)
        return short_filename_with_ext


    # Helpers
    # =======

    def _full_path(self, partial_short):
        if partial_short.startswith("/"):           # partial path starts with '/' ? remove '/'
            partial_short = partial_short[1:]

        shorts = partial_short.split("/")           # split partial path to individual items
        longs = []

        for short in shorts:                        # convert individual short to long parts
            found_long = short                      # if long version won't be found, use short version

            for long_item, short_item in self.long_to_short.items():        # go through the dict
                if short_item == short:             # if current item is the short part we're looking for, use it and quit inner loop
                    found_long = long_item
                    break

            longs.append(found_long)                # append to list of long paths

        partial_long = "/".join(longs)              # list of long filenames to path

        self.log.debug("{} -> {}".format(partial_short, partial_long))

        path = os.path.join(self.root, partial_long)    # prepend root path to partial path
        return path

    # Filesystem methods
    # ==================

    def access(self, path, mode):
        self.log.debug("access {}".format(path))

        full_path = self._full_path(path)
        if not os.access(full_path, mode):
            raise FuseOSError(errno.EACCES)

    def chmod(self, path, mode):
        self.log.debug("chmod {}".format(path))

        full_path = self._full_path(path)
        return os.chmod(full_path, mode)

    def chown(self, path, uid, gid):
        self.log.debug("chown {}".format(path))

        full_path = self._full_path(path)
        return os.chown(full_path, uid, gid)

    def getattr(self, path, fh=None):
        self.log.debug("getattr {}".format(path))

        full_path = self._full_path(path)

        st = os.lstat(full_path)
        return dict((key, getattr(st, key)) for key in ('st_atime', 'st_ctime',
                     'st_gid', 'st_mode', 'st_mtime', 'st_nlink', 'st_size', 'st_uid'))

    def readdir(self, path, fh):
        self.log.debug("readdir {}".format(path))

        full_path = self._full_path(path)

        dirents = ['.', '..']

        if os.path.isdir(full_path):
            dirents.extend(os.listdir(full_path))

        for r in dirents:
            r_short = self.get_short_filename_with_ext(r)
            self.log.debug("readdir {} : {} -> {}".format(path, r, r_short))
            yield r_short

    def readlink(self, path):
        self.log.debug("readlink {}".format(path))

        pathname = os.readlink(self._full_path(path))
        if pathname.startswith("/"):
            # Path name is absolute, sanitize it.
            return os.path.relpath(pathname, self.root)
        else:
            return pathname

    def mknod(self, path, mode, dev):
        self.log.debug("mknod {}".format(path))

        return os.mknod(self._full_path(path), mode, dev)

    def rmdir(self, path):
        self.log.debug("rmdir {}".format(path))

        full_path = self._full_path(path)
        return os.rmdir(full_path)

    def mkdir(self, path, mode):
        self.log.debug("mkdir {}".format(path))

        return os.mkdir(self._full_path(path), mode)

    def statfs(self, path):
        self.log.debug("statfs {}".format(path))

        full_path = self._full_path(path)
        stv = os.statvfs(full_path)
        return dict((key, getattr(stv, key)) for key in ('f_bavail', 'f_bfree',
            'f_blocks', 'f_bsize', 'f_favail', 'f_ffree', 'f_files', 'f_flag',
            'f_frsize', 'f_namemax'))

    def unlink(self, path):
        self.log.debug("unlink {}".format(path))

        return os.unlink(self._full_path(path))

    def symlink(self, name, target):
        self.log.debug("symlink {} -> {}".format(name, target))

        return os.symlink(target, self._full_path(name))

    def rename(self, old, new):
        self.log.debug("rename {} -> {}".format(old, new))

        return os.rename(self._full_path(old), self._full_path(new))

    def link(self, target, name):
        self.log.debug("link {} -> {}".format(target, name))

        return os.link(self._full_path(name), self._full_path(target))

    def utimens(self, path, times=None):
        self.log.debug("utimens {}".format(path))

        return os.utime(self._full_path(path), times)

    # File methods
    # ============

    def open(self, path, flags):
        full_path = self._full_path(path)
        fh = os.open(full_path, flags)

        self.log.debug("open {} -> fh={}".format(path, fh))

        return fh

    def create(self, path, mode, fi=None):
        self.log.debug("create {}".format(path))

        uid, gid, pid = fuse_get_context()
        full_path = self._full_path(path)
        fd = os.open(full_path, os.O_WRONLY | os.O_CREAT, mode)
        os.chown(full_path,uid,gid) #chown to context uid & gid
        return fd

    def read(self, path, length, offset, fh):
        self.log.debug("read {} B from fh={}".format(length, fh))

        ret = None

        try:
            os.lseek(fh, offset, os.SEEK_SET)
            ret = os.read(fh, length)
        except Exception as ex:
            self.log.debug("read fh={} exception: {}".format(fh, str(ex)))
            raise       # re-raise the exception

        return ret

    def write(self, path, buf, offset, fh):
        self.log.debug("write {} B to fh={}".format(len(buf), fh))

        os.lseek(fh, offset, os.SEEK_SET)
        return os.write(fh, buf)

    def truncate(self, path, length, fh=None):
        self.log.debug("truncate {}".format(path))

        full_path = self._full_path(path)
        with open(full_path, 'r+') as f:
            f.truncate(length)

    def flush(self, path, fh):
        self.log.debug("flush fh={}".format(fh))

        ret = 0

        try:
            ret = os.fsync(fh)
        except Exception as ex:
            self.log.debug("flush fh={}, ignoring exception: {}".format(fh, str(ex)))

        return ret

    def release(self, path, fh):
        self.log.debug("release fh={}".format(fh))

        ret = None

        try:
            ret = os.close(fh)
        except Exception as ex:
            self.log.debug("release fh={} exception: {}".format(fh, str(ex)))
            raise       # re-raise the exception

        return ret

    def fsync(self, path, fdatasync, fh):
        self.log.debug("fsync fh={}".format(fh))

        return self.flush(path, fh)


def main(mountpoint, root):
    FUSE(Fuse83(root), mountpoint, nothreads=True, foreground=True, allow_other=True)


if __name__ == '__main__':
    main(sys.argv[2], sys.argv[1])

