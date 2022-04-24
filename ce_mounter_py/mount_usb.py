import os
import shutil
import subprocess
import logging
from shared import print_and_log, setting_get_bool, setting_get_int, \
    get_drives_bitno_from_settings, get_mount_path_for_letter, get_free_letters, delete_files,\
    DEV_DISK_DIR, MOUNT_DIR_RAW


def get_usb_devices():
    """ Look for all the attached disks to the system and return only those attached via usb.
    Return only root device (e.g. /dev/sda), not the individual partitions (e.g. not /dev/sda1) """

    if not os.path.exists(DEV_DISK_DIR):            # the dir doesn't exist? quit now
        return set()

    devs = os.listdir(DEV_DISK_DIR)

    root_devs = set()           # root devices, e.g. /dev/sda
    part_devs = set()           # partition devices, e.g. /dev/sda1, /dev/sda2

    for dev in devs:                                # go through found devices
        dev_name = os.path.join(DEV_DISK_DIR, dev)  # usb-drive -> /dev/disk/by-id/usb-drive

        if 'usb' not in dev_name:                   # if not usb device, then skip it
            continue

        dev_path = os.readlink(dev_name)            # /dev/disk/by-id/usb-drive -> ../../sda1
        dev_path = os.path.join(DEV_DISK_DIR, dev_path)  # ../../sda1 -> /dev/disk/by-id/../../sda1
        dev_path = os.path.abspath(dev_path)        # /dev/disk/by-id/../../sda1 -> /dev/sda1

        if any(char.isdigit() for char in dev_path):     # pointing to partition (e.g. sda1), add to part devices?
            part_devs.add(dev_path)
        else:                                       # pointing to root device? (e.g. sda) add to root devices
            root_devs.add(dev_path)

    return root_devs, part_devs


def get_mounts():
    """ See what devices are already mounted in this system, return them as tuples of device + mount point. """
    result = subprocess.run(['mount'], stdout=subprocess.PIPE)        # run 'mount' command
    result = result.stdout.decode('utf-8')  # get output as string
    lines = result.split('\n')              # split whole result to lines

    mounts = []

    for line in lines:                      # go through lines
        if not line.startswith('/dev/'):    # not a /dev mount? skip it
            continue

        parts = line.split(' ')             # split '/dev/sda1 on /mnt/drive type ...' to items
        dev_dir = parts[0], parts[2]        # get device and mount point
        mounts.append(dev_dir)

    # return the mounts
    return mounts


def get_symlinked_mount_folders():
    """ go through our MOUNT_DIR_TRANS, find out which dirs are just a symlink to other mount points and return them """

    # get letters from config, convert them to bit numbers
    first, shared, config, zip_ = get_drives_bitno_from_settings()

    symlinks = []

    for i in range(16):                     # go through available drive letters - from 0 to 15
        if i < first:                       # below first char? skip it
            continue

        if i in [shared, config, zip_]:     # skip these special drives
            continue

        letter = chr(97 + i)                # bit number to ascii char

        # check if position at id_ is used or not
        path = get_mount_path_for_letter(letter)    # construct path where the drive letter should be mounted

        if not os.path.exists(path):        # if mount point doesn't exist, skip it
            continue

        if not os.path.islink(path):        # not a link? skip it
            continue

        # the path exists and it's a link, store original mount path vs atari mount path
        source_path = os.readlink(path)
        source_dest = source_path, path
        symlinks.append(source_dest)

    return symlinks


def to_be_linked(mounts, symlinked):
    """ get folders which should be symlinked
    :param mounts: list of tuples device-vs-mount_dir
    :param symlinked: list of tuples mount_dir-vs-symlink_destination
    """

    link_these = []

    for dev, mountdir in mounts:        # go through the list of mounted devices
        found = False

        if mountdir in ['/']:           # don't symlink these linux folders
            continue

        # also don't symlink folder starting with these
        ignore = False
        for skip in ['/boot', '/run', '/proc', '/var', '/etc', '/opt']:
            if mountdir.startswith(skip):
                ignore = True

        if ignore:                      # should ignore this mount?
            continue

        for source, dest in symlinked:  # see if this mountdir has been already symlinked
            if mountdir == source:      # the mount dir is the same as source of some link? found this mount in symlink
                found = True
                break

        if not found:                   # this mountdir wasn't found, add it to those which should be linked
            link_these.append(mountdir)

    return link_these


def get_not_mounted_devices(devs, mounts_in):
    """ Go through the list of devices (devs) and through list of mounts (mnts), and if the device from devs is not
    found in the already mounted devices, it will be returned as a list with others. """
    devices_not_mounted = []

    for device in devs:                     # set of devices, e.g. '/dev/sdd'
        found = False

        for mnt_dev, _ in mounts_in:        # list of devices and mounts, e.g. ('/dev/sdd1', '/media/drive')
            if device in mnt_dev:           # if device found in one of the mounted devices, mark that we found it
                found = True
                break

        if not found:           # not found? use it
            devices_not_mounted.append(device)  # not mounted, store it

    return devices_not_mounted


def get_mount_path_for_id(id_):
    path = os.path.join(MOUNT_DIR_RAW, str(id_))  # construct path where the id_ should be mounted
    return path


def get_free_ids():
    """ Check which device IDs have RAW type (2) selected, check which of those are already used by some device
    and return only those which are free and can be used. """
    ids = []

    for id_ in range(8):
        key = f'ACSI_DEVTYPE_{id_}'

        # if id_ is not RAW, skip it
        if setting_get_int(key) != 2:
            continue

        # check if position at id_ is used or not
        path = get_mount_path_for_id(id_)

        if not os.path.exists(path):        # file/link doesn't exist or it's broken? it's free
            ids.append(id_)

            try:
                os.unlink(path)             # try to delete it if it exists (e.g. if broken)
            except Exception as exc:
                print_and_log(logging.DEBUG, f"unlink {path} failed (might be ok): {str(exc)}")

    return ids


def is_raw_device_linked(device):
    """ check if this device is already symlinked or not """
    for id_ in range(8):
        path = get_mount_path_for_id(id_)

        if not os.path.exists(path) or not os.path.islink(path):    # path doesn't exist or not a link, skip rest
            continue

        source = os.readlink(path)  # get link source

        if device == source:        # this device is source of this link, we got this device
            return True

    return False


def link_raw_device(device):
    # find which ACSI slots are configured as RAW and are free
    ids = get_free_ids()

    if not ids:
        print_and_log(logging.WARNING, f"No free RAW IDs found, cannot mount device: {device}")
        return False

    id_ = ids[0]  # get 0th available id
    path = get_mount_path_for_id(id_)  # create mount path for this id

    # create symlink from device to ACSI slot
    print_and_log(logging.DEBUG, f"mount_device as RAW: {device} -> {path}")
    os.symlink(device, path)


def mount_device_translated(mounts, device):
    # find empty device letters
    letters = get_free_letters(mounts)

    if not letters:
        print_and_log(logging.WARNING, f"No free translated letters found, cannot mount device: {device}")
        return False

    letter = letters[0]  # get 0th letter
    path = get_mount_path_for_letter(letter)  # construct path where the drive letter should be mounted

    # mount device to expected path
    print_and_log(logging.DEBUG, f"mount_device as TRANSLATED: {device} -> {path}")

    os.makedirs(path, exist_ok=True)  # make mount dir if it doesn't exist

    logfile = "/tmp/mount.log"  # define log files, delete them before mount
    logfile2 = os.path.join(path, os.path.basename(logfile))
    delete_files([logfile, logfile2])

    mount_cmd = f"mount -v {device} {path} > {logfile} 2>&1"
    status = os.system(mount_cmd)  # do the mount

    if status != 0:  # some error? then copy in the log files
        shutil.copy(logfile, logfile2)
        print_and_log(logging.WARNING, f'mount of {device} failed, log copied')


def symlink_dir(mounts, mountdir):
    """ create a symlink from existing mounted dir to our atari mount dir """
    # find empty device letters
    letters = get_free_letters(mounts)

    if not letters:
        print_and_log(logging.WARNING, f"No free translated letters found, cannot symlink dir: {mountdir}")
        return False

    letter = letters[0]  # get 0th letter
    path = get_mount_path_for_letter(letter)  # construct path where the drive letter should be mounted

    print_and_log(logging.DEBUG, f"symlink_dir: {mountdir} -> {path}")

    # symlink mounted dir into our atari mount path
    os.symlink(mountdir, path)


def find_and_mount_devices():
    """ look for USB devices, find those which are not mounted yet, find a mount point for them, mount them """
    mount_raw_not_trans = setting_get_bool('MOUNT_RAW_NOT_TRANS')
    print_and_log(logging.INFO, f"MOUNT mode: {'RAW' if mount_raw_not_trans else 'TRANS'}")

    root_devs, part_devs = get_usb_devices()            # get attached USB devices
    print_and_log(logging.INFO, f'devices: {root_devs}')

    if mount_raw_not_trans:         # for raw mount, check if symlinked
        root_devs = [dev for dev in root_devs if not is_raw_device_linked(dev)]     # keep only not symlinked devices

        for device in root_devs:    # mount all the found partition devices, if possible
            link_raw_device(device)
    else:                           # for translated mounts
        mounts = get_mounts()                               # get devices and their mount points
        print_and_log(logging.INFO, f'mounts: {mounts}')

        symlinked = get_symlinked_mount_folders()           # get folders which are already symlinked
        link_these = to_be_linked(mounts, symlinked)        # get folders which should be symlinked

        root_devs_not_mounted = get_not_mounted_devices(root_devs, mounts)  # filter out devices which are already mounted
        print_and_log(logging.INFO, f'not mounted: {root_devs_not_mounted}')

        # from root device get device for partition (e.g. from sdd -> sdd1, sdd2)
        for root_dev in root_devs_not_mounted:          # go through the not-mounted devices and try to mount them
            devices = [part_dev for part_dev in part_devs if root_dev in part_dev]
            print_and_log(logging.DEBUG, f"for root device {root_dev} found partition devices {devices}")

            for device in devices:      # mount all the found partition devices, if possible
                mount_device_translated(mounts, device)

        for mountdir in link_these:  # link these mount dirs into atari mount path
            symlink_dir(mounts, mountdir)
