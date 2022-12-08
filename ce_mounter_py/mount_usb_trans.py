import os
import logging
from shared import print_and_log, get_symlink_path_for_letter, \
    get_free_letters, DEV_DISK_DIR, LOG_DIR, unlink_without_fail, symlink_if_needed, get_disk_partitions, \
    get_root_fs_device


def get_usb_devices():
    """ Look for all the attached disks to the system and return only those attached via usb.
    Return:
         - only root devices (e.g. /dev/sda) - for mounting them as RAW
         - the individual partitions (e.g. /dev/sda1) - for mounting them as TRANSLATED
    """

    dev_root_fs = get_root_fs_device()              # get device which is used for root fs on this linux box

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

        if dev_path.startswith(dev_root_fs):        # don't report any partition of device which is used for root fs
            continue

        if any(char.isdigit() for char in dev_path):     # pointing to partition (e.g. sda1), add to part devices?
            part_devs.add(dev_path)
        else:                                       # pointing to root device? (e.g. sda) add to root devices
            root_devs.add(dev_path)

    return root_devs, part_devs


def get_mounts():
    """ See what devices are already mounted in this system, return them as tuples of device + mount point. """

    dev_root_fs = get_root_fs_device()              # get device which is used for root fs on this linux box

    partitions = get_disk_partitions()              # get existing mounted partitions
    mounts = []

    for part in partitions:
        # not a /dev mount or root fs mount? skip it
        if not part.device.startswith('/dev/') or part.mountpoint == '/':
            continue

        # ignore devices starting with these DEVICE paths
        if any(part.device.startswith(ignored) for ignored in ['/dev/loop', '/snap/', dev_root_fs]):
            continue

        # ignore devices starting with these MOUNT POINTS
        if any(part.mountpoint.startswith(ignored) for ignored in ['/boot', '/run']):
            continue

        dev_dir = part.device, part.mountpoint  # get device and mount point
        mounts.append(dev_dir)

    # return the mounts
    return mounts


def get_not_mounted_devices(devs, mounts_in):
    """ Go through the list of devices and through list of mounts, and if the device from devs is not
    found in the already mounted devices, it will be returned as a list of devices, which are not mounted

    :param devs: set of devices (e.g. /dev/sda) which are present on this machine
    :param mounts_in: list of tuples, each tuple being device and its mount point
    :return: list of not mounted devices (== subset of devs)
    """
    print_and_log(logging.DEBUG, f"get_not_mounted_devices: devs: {devs}, mounts_in: {mounts_in}")

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


def mount_device_to_mnt(device):
    """ take the found device and mount it to /mnt/ folder """

    just_dev_name = os.path.basename(device)            # '/dev/sda1' -> 'sda1'
    mount_dir = os.path.join("/mnt", just_dev_name)     # '/mnt' + 'sda1' = /mnt/sda1
    os.makedirs(mount_dir, exist_ok=True)               # create mount dir if it doesn't exist

    mnt_log_file = os.path.join(LOG_DIR, f'mount_{just_dev_name}.log')

    mount_cmd = f"mount -v {device} {mount_dir} > {mnt_log_file} 2>&1"
    print_and_log(logging.INFO, f'mount_device_to_mnt: mount_cmd: {mount_cmd}')

    status = os.system(mount_cmd)       # do the mount

    # some error?
    if status != 0:
        print_and_log(logging.WARNING, f'mount of {device} failed')
        return None

    # on success
    print_and_log(logging.INFO, f'mount of {device} succeeded')
    return mount_dir


def mount_not_mounted_devices(root_devs, part_devs, mounts):
    """ Function will look at the found root devices, compare then to mounted devices, then try to mount
    those which are not mounted. Then will return list of newly mounted dirs. """

    root_devs_not_mounted = get_not_mounted_devices(root_devs, mounts)  # filter out devices which are already mounted
    print_and_log(logging.INFO, f'not mounted: {root_devs_not_mounted}')

    # for each not mounted root device (e.g. /dev/sdd) get all the partition devices
    # which should be mounted (e.g. /dev/sdd1, /dev/sdd2) and try to mount those not mounted partition devices
    newly_mounted = []

    for root_dev in root_devs_not_mounted:
        devices = [part_dev for part_dev in part_devs if root_dev in part_dev]
        print_and_log(logging.DEBUG, f"for root device {root_dev} found partition devices {devices}")

        for device in devices:              # mount all the found partition devices, if possible
            mount_dir = mount_device_to_mnt(device)

            if mount_dir:                   # if successfully mounted a device, store the path
                newly_mounted.append(mount_dir)

    return newly_mounted


def symlink_not_linked(mounted_all):
    """ Go through all the mounted dirs supplied, go through all the symlinked dirs and skip those which are
    already symlinked, do link those which are not symlinked yet """

    letters, sources_existing = get_free_letters()          # get free letters, also removes dead symlinks
    sources_not_linked = mounted_all - sources_existing     # get only not linked sources
    print_and_log(logging.DEBUG, f'mounted_all       : {mounted_all}')
    print_and_log(logging.DEBUG, f'sources_existing  : {sources_existing}')
    print_and_log(logging.DEBUG, f'sources_not_linked: {sources_not_linked}')

    # find out if we're able to symlink all the mounts
    len_letters = len(letters)
    len_not_linked = len(sources_not_linked)
    len_can = min(len_letters, len_not_linked)  # we can symlink only this count

    if len_letters < len_not_linked:
        print_and_log(logging.WARNING, (f'symlink_not_linked: not enough letters to symlink all not linked mounts. '
                                        f'(letters) {len_letters} < {len_not_linked} (not linked)'))

    sources_not_linked = list(sources_not_linked)       # set to list, so we can use [i] below

    # now symlink of mounts to expected symlinks path
    for i in range(len_can):            # symlink all we can
        letter = letters[i]             # get free letter
        path = get_symlink_path_for_letter(letter)  # construct path where the drive letter should be mounted
        source = sources_not_linked[i]          # get path we should symlink from

        # symlink mounted dir into our atari mount path
        print_and_log(logging.DEBUG, f"symlink_not_linked: {source} -> {path}")

        try:
            symlink_if_needed(source, path)

            if not os.path.exists(path):    # check if created symlink exists and it's not broken
                print_and_log(logging.WARNING, f"symlink_not_linked: symlink {source} -> {path} created but broken!")
                unlink_without_fail(path)   # delete the broken symlink
        except Exception as ex:
            print_and_log(logging.WARNING, f"symlink_not_linked: failed to symlink {source} -> {path}: {str(ex)}")


def find_and_mount_translated(root_devs, part_devs):
    """
    Function will go through the found devices, checks if any of them needs to be mounted and mounts those,
    then symlinks the mounted devices to expected folder for translated drives.

    :param root_devs: set of root devices (e.g. /dev/sda) - those without partition numbers
    :param part_devs: set of partition devices (e.g. /dev/sda1) - those which point to partition in device
    """

    mounts = get_mounts()        # get devices and their mount points
    print_and_log(logging.INFO, f'mounts: {mounts}')

    # mount every device that is not mounted yet to /mnt/ folder
    mounted_new = mount_not_mounted_devices(root_devs, part_devs, mounts)
    print_and_log(logging.DEBUG, f'mounted_new: {mounted_new}')

    # get just mount points for the old (existing) mounts
    mounted_old = [mount[1] for mount in mounts]
    print_and_log(logging.DEBUG, f'mounted_old: {mounted_old}')

    # all the mounted folders (e.g. [/mnt/sda1, /mnt/sdb])
    mounted_all = set(mounted_old + mounted_new)
    print_and_log(logging.DEBUG, f'mounted_all: {mounted_all}')

    # now symlink everything that is not symlinked yet
    symlink_not_linked(mounted_all)
