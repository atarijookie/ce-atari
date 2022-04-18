from copy import deepcopy
import os
import re
import threading
import shutil
import pyinotify
import asyncio
from setproctitle import setproctitle
from time import sleep
import subprocess
import logging
from logging.handlers import RotatingFileHandler

settings_path = "/ce/settings"          # path to settings dir

settings_default = {'DRIVELETTER_FIRST': 'C', 'MOUNT_RAW_NOT_TRANS': 0, 'SHARED_ENABLED': 0, 'SHARED_NFS_NOT_SAMBA': 0,
                    'ACSI_DEVTYPE_0': 0, 'ACSI_DEVTYPE_1': 1, 'ACSI_DEVTYPE_2': 0, 'ACSI_DEVTYPE_3': 0,
                    'ACSI_DEVTYPE_4': 0, 'ACSI_DEVTYPE_5': 0, 'ACSI_DEVTYPE_6': 0, 'ACSI_DEVTYPE_7': 0}

settings = {}

MOUNT_COMMANDS_DIR = '/tmp/ce/cmds'
MOUNT_DIR_RAW = '/tmp/ce/raw'
MOUNT_DIR_TRANS = '/tmp/ce/trans'
LETTER_SHARED = 'N'                     # network drive on N
LETTER_CONFIG = 'O'                     # config drive on O
LETTER_ZIP = 'P'                        # ZIP file drive on P

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


def setting_get_int(setting_name):
    global settings

    value = 0
    value_raw = settings.get(setting_name)

    try:
        value = int(value_raw)
    except Exception as exc:
        print_and_log(logging.WARNING, f"failed to convert {value} to int: {str(exc)}")

    return value


def get_usb_devices():
    """ Look for all the attached disks to the system and return only those attached via usb.
    Return only root device (e.g. /dev/sda), not the individual partitions (e.g. not /dev/sda1) """

    if not os.path.exists(dev_disk_dir):            # the dir doesn't exist? quit now
        return set()

    devs = os.listdir(dev_disk_dir)

    root_devs = set()           # root devices, e.g. /dev/sda
    part_devs = set()           # partition devices, e.g. /dev/sda1, /dev/sda2

    for dev in devs:                                # go through found devices
        dev_name = os.path.join(dev_disk_dir, dev)  # usb-drive -> /dev/disk/by-id/usb-drive

        if 'usb' not in dev_name:                   # if not usb device, then skip it
            continue

        dev_path = os.readlink(dev_name)            # /dev/disk/by-id/usb-drive -> ../../sda1
        dev_path = os.path.join(dev_disk_dir, dev_path)  # ../../sda1 -> /dev/disk/by-id/../../sda1
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


def get_drives_bitno_from_settings():
    """ get letters from config, convert them to bit numbers """

    first = settings_letter_to_bitno('DRIVELETTER_FIRST')
    shared = letter_to_bitno(LETTER_SHARED)     # network drive
    config = letter_to_bitno(LETTER_CONFIG)     # config drive
    zip_ = letter_to_bitno(LETTER_ZIP)          # ZIP file drive

    return first, shared, config, zip_


def settings_letter_to_bitno(setting_name):
    """ get setting by setting name, convert it to integer and then to drive bit number for Atari, e.g. 
    'a' is 0, 'b' is 1, 'p' is 15 """

    drive_letter = settings.get(setting_name, 'c')
    return letter_to_bitno(drive_letter)


def letter_to_bitno(drive_letter):
    """ convert drive letter to integer and then to drive bit number for Atari, e.g.
    'a' is 0, 'b' is 1, 'p' is 15 """

    letter = drive_letter.lower()        # get setting and convert it to lowercase
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
    first, shared, config, zip_ = get_drives_bitno_from_settings()

    for i in range(16):                     # go through available drive letters - from 0 to 15
        if i < first:                       # below first char? skip it
            continue

        if i in [shared, config, zip_]:     # skip these special drives
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

                if os.path.islink(path) and os.path.exists(path):       # if this is a link and it's not broken
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


def handle_read_callback(notifier):
    print_and_log(logging.INFO, f'monitored folder changed...')

    find_and_mount_devices()        # find devices and mount them


def get_mount_zip_cmd():
    """ get which ZIP file should be mounted """

    path_ = os.path.join(MOUNT_COMMANDS_DIR, "mount_zip")

    if not os.path.exists(path_):       # no file like this exists? quit
        return None

    try:
        with open(path_) as f:          # open and read file
            line = f.read()

        os.unlink(path_)                # delete the file

        line = line.strip()             # remove whitespaces
        return line
    except Exception as exc:            # on exception
        print_and_log(logging.INFO, f'failed to read {path_} : {str(exc)}')

    return None                         # if got here, it failed


def umount_if_mounted(mount_dir):
    """ check if this dir is mounted and if it is, then umount it """

    try:
        cmd = f'umount {mount_dir}'     # construct umount command
        os.system(cmd)                  # execute umount command
        print_and_log(logging.INFO, f'umount_if_mounted: umounted {mount_dir}')
    except Exception as exc:
        print_and_log(logging.INFO, f'umount_if_mounted: failed to umount {mount_dir} : {str(exc)}')


def options_to_string(opts: dict):
    """ turn dictionary of options into single string """

    opts_strs = []

    for key, value in opts.items():     # go through all the dict items
        if not value:                   # value not provided? skip adding
            continue

        opt = f'{key}={value}'          # from key, value create 'key=value' string
        opts_strs.append(opt)

    if opts_strs:       # if got some options, merge them, prepend with -o
        options = '-o ' + ','.join(opts_strs)
        return options

    return None         # no valid options found


def mount_shared():
    """ this function checks for shared drive settings, mounts drive if needed """
    global settings

    cmd_last = ''

    while True:
        sleep(5)

        shared_enabled = setting_get_bool('SHARED_ENABLED')

        if not shared_enabled:      # if shared drive not enabled, don't do rest
            continue

        addr = settings.get('SHARED_ADDRESS')
        path_ = settings.get('SHARED_PATH')
        user = settings.get('SHARED_USERNAME')
        pswd = settings.get('SHARED_PASSWORD')

        if not addr or not path_:   # address or path not provided? don't do the rest
            continue

        nfs_not_samba = setting_get_bool('SHARED_NFS_NOT_SAMBA')
        mount_path = get_mount_path_for_letter(LETTER_SHARED)       # get where it should be mounted

        if nfs_not_samba:       # NFS shared drive
            options = options_to_string({'username': user, 'password': pswd, 'vers': 3})
            cmd = f'mount -t nfs {options} {addr}:/{path_} {mount_path}'
        else:                   # cifs / samba / windows share
            options = options_to_string({'username': user, 'password': pswd})
            cmd = f'mount -t cifs {options} //{addr}/{path_} {mount_path}'

        # if no change in the created command, nothing to do here
        if cmd == cmd_last:
            continue

        cmd_last = cmd          # store this cmd

        cmd += " > /tmp/ce/mount.log 2>&1 "     # append writing of stdout and stderr to file

        # command changed, we should execute it
        os.makedirs(mount_path, exist_ok=True)  # create dir if not exist
        umount_if_mounted(mount_path)           # possibly umount

        good = False
        try:
            print_and_log(logging.INFO, f'mount_shared: cmd: {cmd}')
            status = os.system(cmd)         # try mounting
            good = status == 0              # good if status is 0
        except Exception as exc:
            print_and_log(logging.INFO, f'mount_shared: mount failed : {str(exc)}')

        if not good:        # mount failed, copy mount log
            try:
                shutil.copy('/tmp/ce/mount.log', mount_path)
            except Exception as exc:
                print_and_log(logging.INFO, f'mount_shared: copy of log file failed : {str(exc)}')


def mount_zip_file(zip_file_path):
    """ mount ZIP file if possible """
    if not os.path.exists(zip_file_path):  # got path, but it doesn't exist?
        return

    mount_path = get_mount_path_for_letter(LETTER_ZIP)  # get where it should be mounted
    umount_if_mounted(mount_path)  # umount dir if it's mounted
    os.makedirs(mount_path, exist_ok=True)  # create mount dir

    mount_cmd = f'fuse-zip {zip_file_path} {mount_path}'  # construct mount command

    try:  # try to mount it
        print_and_log(logging.INFO, f'mount_zip_file: cmd: {mount_cmd}')
        os.system(mount_cmd)
        print_and_log(logging.INFO, f'mounted {zip_file_path} to {mount_path}')
    except Exception as exc:  # on exception
        print_and_log(logging.INFO, f'failed to mount zip with cmd: {mount_cmd} : {str(exc)}')


def mount_on_command():
    """ endless loop to mount things from CE main app on request """
    while True:
        if not os.path.exists(MOUNT_COMMANDS_DIR):              # if dir for commands doesn't exist, create it
            os.makedirs(MOUNT_COMMANDS_DIR, exist_ok=True)

        #  command to mount ZIP?
        zip_file_path = get_mount_zip_cmd()

        if zip_file_path:       # should mount ZIP?
            mount_zip_file(zip_file_path)

        sleep(1)


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

    # start thread for mount-on-command
    thread_mount = threading.Thread(target=mount_on_command, daemon=True)
    thread_mount.start()

    # start thread for mounting shared drive
    thread_shared = threading.Thread(target=mount_shared, daemon=True)
    thread_shared.start()

    # TODO: remove broken linked devices / mounts on removal

    # watch the dev_disk_dir folder, on changes look for devices and mount them
    wm = pyinotify.WatchManager()
    loop = asyncio.get_event_loop()
    notifier = pyinotify.AsyncioNotifier(wm, loop, callback=handle_read_callback)
    wm.add_watch(dev_disk_dir, pyinotify.IN_CREATE | pyinotify.IN_DELETE | pyinotify.IN_UNMOUNT)

    try:
        loop.run_forever()
    except KeyboardInterrupt:
        print_and_log(logging.INFO, '\nterminated by keyboard...')

    notifier.stop()
