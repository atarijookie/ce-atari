from copy import deepcopy
import os
import re
import shutil
import pyinotify
import asyncio
from setproctitle import setproctitle
import subprocess
import logging
from logging.handlers import RotatingFileHandler

settings_path = "/ce/settings"          # path to settings dir

settings_default = {'DRIVELETTER_FIRST': 'C', 'DRIVELETTER_SHARED': 'P', 'DRIVELETTER_CONFDRIVE': 'O',
                    'MOUNT_RAW_NOT_TRANS': 0, 'SHARED_ENABLED': 0, 'SHARED_NFS_NOT_SAMBA': 0,
                    'ACSI_DEVTYPE_0': 0, 'ACSI_DEVTYPE_1': 1, 'ACSI_DEVTYPE_2': 0, 'ACSI_DEVTYPE_3': 0,
                    'ACSI_DEVTYPE_4': 0, 'ACSI_DEVTYPE_5': 0, 'ACSI_DEVTYPE_6': 0, 'ACSI_DEVTYPE_7': 0}

settings = {}

MOUNT_DIR_RAW = '/tmp/ce/raw'
MOUNT_DIR_TRANS = '/tmp/ce/trans'

dev_disk_dir = '/dev/disk/by-path'


def print_and_log(loglevel, message):
    print(message)
    app_log.log(loglevel, message)


def settings_load():
    """ load all the present settings from the settings dir """

    global settings
    settings = deepcopy(settings_default)  # fill settings with default values before loading

    os.makedirs(settings_path, exist_ok=True)

    for f in os.listdir(settings_path):         # go through the settings dir
        path = os.path.join(settings_path, f)   # create full path

        if not os.path.isfile(path):            # if it's not a file, skip it
            continue

        with open(path, "r") as file:           # read the file into value in dictionary
            value = file.readline()
            value = re.sub('[\n\r\t]', '', value)
            settings[f] = value


def setting_get_bool(setting_name):
    global settings

    value = False
    value_raw = settings.get(setting_name)

    try:
        value = bool(int(value_raw))
    except Exception as exc:
        print_and_log(logging.WARNING, f"failed to convert {value} to bool: {str(exc)}")

    return value


def get_usb_devices():
    """ Look for all the attached disks to the system and return only those attached via usb.
    Return only root device (e.g. /dev/sda), not the individual partitions (e.g. not /dev/sda1) """

    if not os.path.exists(dev_disk_dir):            # the dir doesn't exist? quit now
        return set()

    devs = os.listdir(dev_disk_dir)

    root_devs = set()

    for dev in devs:                                # go through found devices
        dev_name = os.path.join(dev_disk_dir, dev)  # usb-drive -> /dev/disk/by-id/usb-drive

        if 'usb' not in dev_name:                   # if not usb device, then skip it
            continue

        dev_path = os.readlink(dev_name)            # /dev/disk/by-id/usb-drive -> ../../sda1
        dev_path = os.path.join(dev_disk_dir, dev_path)  # ../../sda1 -> /dev/disk/by-id/../../sda1
        dev_path = os.path.abspath(dev_path)        # /dev/disk/by-id/../../sda1 -> /dev/sda1

        if any(char.isdigit() for char in dev_path):     # if this is pointing to partition (e.g. sda1), ignore it
            continue

        root_devs.add(dev_path)

    return root_devs


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
    first = settings_letter_to_bitno('DRIVELETTER_FIRST')
    shared = settings_letter_to_bitno('DRIVELETTER_SHARED')
    config = settings_letter_to_bitno('DRIVELETTER_CONFDRIVE')

    symlinks = []

    for i in range(16):                     # go through available drive letters - from 0 to 15
        if i < first:                       # below first char? skip it
            continue

        if i == shared or i == config:      # skip these two special drives
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
        if settings.get(key) != 2:
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


def settings_letter_to_bitno(setting_name):
    """ get setting by setting name, convert it to integer and then to drive bit number for Atari, e.g. 
    'a' is 0, 'b' is 1, 'p' is 15 """

    letter = settings.get(setting_name, 'c').lower()        # get setting and convert it to lowercase
    letter = letter[:1]         # get only 0th character
    ascii_no = ord(letter)      # letter to ascii number ('c' -> 99)
    bitno = ascii_no - 97       # ascii number to bit number (99 -> 2)
    return bitno


def get_mount_path_for_letter(letter):
    path = os.path.join(MOUNT_DIR_TRANS, letter)  # construct path where the drive letter should be mounted
    return path


def get_free_letters(mounts_in):
    """ Check which Atari drive letters are free and return them. """
    letters_out = []

    # get letters from config, convert them to bit numbers
    first = settings_letter_to_bitno('DRIVELETTER_FIRST')
    shared = settings_letter_to_bitno('DRIVELETTER_SHARED')
    config = settings_letter_to_bitno('DRIVELETTER_CONFDRIVE')

    for i in range(16):                     # go through available drive letters - from 0 to 15
        if i < first:                       # below first char? skip it
            continue

        if i == shared or i == config:      # char reserved for shared or config? skip it
            continue

        letter = chr(97 + i)                # bit number to ascii char

        # check if position at id_ is used or not
        path = get_mount_path_for_letter(letter)    # construct path where the drive letter should be mounted

        if not os.path.exists(path):        # if mount point doesn't exist, it's free
            letters_out.append(letter)
        else:                               # mount point exists, but it might not be used
            found = False

            for _, mnt_point in mounts_in:  # list of devices and mounts, e.g. ('/dev/sdd1', '/media/drive')
                if path == mnt_point:       # if this path is one of the mounted paths, mark that we found it
                    found = True
                    break

                if os.path.islink(path):    # if this is a link
                    # TODO: check if link is not broken
                    found = True
                    break

            if not found:                   # not found? use it
                letters_out.append(letter)

    return letters_out


def delete_files(files):
    for file in files:
        try:
            os.unlink(file)
        except Exception as exc:
            print_and_log(logging.WARNING, f'failed to delete {file} : {str(exc)}')


def mount_device_raw(device):
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

    # TODO: from root device get device for partition (e.g. from sdd -> sdd1)

    mount_cmd = f"mount -v {device} {path} > {logfile} 2>&1"
    status = os.system(mount_cmd)  # do the mount

    if status != 0:  # some error? then copy in the log files
        shutil.copy(logfile, logfile2)
        print_and_log(logging.WARNING, f'mount of {device} failed, log copied')


def symlink_dir(mounts, mountdir):
    # find empty device letters
    letters = get_free_letters(mounts)

    if not letters:
        print_and_log(logging.WARNING, f"No free translated letters found, cannot mount device: {device}")
        return False

    letter = letters[0]  # get 0th letter
    path = get_mount_path_for_letter(letter)  # construct path where the drive letter should be mounted

    # TODO: symlink mountdir


def mount_device(mount_raw_not_trans, mounts, device):
    if mount_raw_not_trans:         # mount USB as RAW?
        mount_device_raw(device)
    else:                           # mount USB as translated?
        mount_device_translated(mounts, device)


def find_and_mount_devices():
    """ look for USB devices, find those which are not mounted yet, find a mount point for them, mount them """
    mount_raw_not_trans = setting_get_bool('MOUNT_RAW_NOT_TRANS')
    print_and_log(logging.INFO, f"MOUNT mode: {'RAW' if mount_raw_not_trans else 'TRANS'}")

    devices = get_usb_devices()     # get attached USB devices
    mounts = get_mounts()           # get devices and their mount points
    print_and_log(logging.INFO, f'devices: {devices}')
    print_and_log(logging.INFO, f'mounts: {mounts}')

    symlinked = get_symlinked_mount_folders()           # get folders which are already symlinked
    link_these = to_be_linked(mounts, symlinked)        # get folders which should be symlinked

    devices = get_not_mounted_devices(devices, mounts)      # filter out devices which are already mounted
    print_and_log(logging.INFO, f'not mounted: {devices}')

    for device in devices:          # go through the not-mounted devices and try to mount them
        mount_device(mount_raw_not_trans, mounts, device)

    if not mount_raw_not_trans:         # if translated mounting enabled
        for mountdir in link_these:     # link these mount dirs into atari mount path
            symlink_dir(mountdir)


def handle_read_callback(notifier):
    print_and_log(logging.INFO, f'monitored folder changed...')

    find_and_mount_devices()        # find devices and mount them


if __name__ == "__main__":
    setproctitle("ce_mounter")      # set process title

    log_formatter = logging.Formatter('%(asctime)s %(levelname)s %(funcName)s(%(lineno)d) %(message)s')

    my_handler = RotatingFileHandler('/tmp/ce_mounter.log', mode='a', maxBytes=1024 * 1024, backupCount=1)
    my_handler.setFormatter(log_formatter)
    my_handler.setLevel(logging.DEBUG)

    app_log = logging.getLogger()
    app_log.setLevel(logging.DEBUG)
    app_log.addHandler(my_handler)

    os.makedirs(MOUNT_DIR_RAW, exist_ok=True)
    os.makedirs(MOUNT_DIR_TRANS, exist_ok=True)

    settings_load()                 # load settings from disk

    print_and_log(logging.INFO, f'On start will look for not mounted devices - they might be already connected')
    find_and_mount_devices()        # find devices and mount them

    print_and_log(logging.INFO, f'Will now start watching folder: {dev_disk_dir}')

    # TODO: mount shared drive

    # watch the dev_disk_dir folder, on changes look for devices and mount them
    wm = pyinotify.WatchManager()
    loop = asyncio.get_event_loop()
    notifier = pyinotify.AsyncioNotifier(wm, loop, callback=handle_read_callback)
    wm.add_watch(dev_disk_dir, pyinotify.IN_CREATE | pyinotify.IN_DELETE | pyinotify.IN_UNMOUNT)
    loop.run_forever()
    notifier.stop()
