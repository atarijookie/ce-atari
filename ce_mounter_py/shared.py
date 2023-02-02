from copy import deepcopy
import os
import sys
import re
import json
import psutil
import subprocess
import socket
from functools import partial
#from wrapt_timeout_decorator import timeout
from loguru import logger as app_log


KEY_LETTER_FIRST = 'DRIVELETTER_FIRST'
KEY_LETTER_CONFDRIVE = 'DRIVELETTER_CONFDRIVE'
KEY_LETTER_SHARED = 'DRIVELETTER_SHARED'

DEV_TYPE_OFF = 0
DEV_TYPE_SD = 1
DEV_TYPE_RAW = 2
DEV_TYPE_CEDD = 3

settings_default = {KEY_LETTER_FIRST: 'C', KEY_LETTER_CONFDRIVE: 'O', KEY_LETTER_SHARED: 'N',
                    'MOUNT_RAW_NOT_TRANS': 0, 'SHARED_ENABLED': 0, 'SHARED_NFS_NOT_SAMBA': 0,
                    'ACSI_DEVTYPE_0': DEV_TYPE_OFF, 'ACSI_DEVTYPE_1': DEV_TYPE_CEDD, 'ACSI_DEVTYPE_2': DEV_TYPE_OFF,
                    'ACSI_DEVTYPE_3': DEV_TYPE_OFF, 'ACSI_DEVTYPE_4': DEV_TYPE_OFF, 'ACSI_DEVTYPE_5': DEV_TYPE_OFF,
                    'ACSI_DEVTYPE_6': DEV_TYPE_OFF, 'ACSI_DEVTYPE_7': DEV_TYPE_OFF}

MOUNT_SHARED_CMD_LAST = os.path.join(os.getenv('DATA_DIR'), 'mount_shared_cmd_last')
FILE_MOUNT_CMD_SAMBA = os.path.join(os.getenv('SETTINGS_DIR'), 'mount_cmd_samba.txt')
FILE_MOUNT_CMD_NFS = os.path.join(os.getenv('SETTINGS_DIR'), 'mount_cmd_nfs.txt')

FILE_OLD_SETTINGS = os.path.join(os.getenv('DATA_DIR'), 'settings_old.json')         # where the old settings in json are
FILE_ROOT_DEV = os.path.join(os.getenv('DATA_DIR'), 'root_dev.txt')                  # what device holds root file system

DEV_DISK_DIR = '/dev/disk/by-path'

MOUNT_DIR_SHARED = os.getenv('MOUNT_DIR_SHARED')        # where the shared drive will be mounted
MOUNT_DIR_ZIP_FILE = os.getenv('MOUNT_DIR_ZIP_FILE')    # where the ZIP file will be mounted
MOUNTED_ZIP_FILE_LINK = os.path.join(os.getenv('DATA_DIR'), "mounted_zip_file.lnk")


def letter_shared():
    return settings_letter(KEY_LETTER_SHARED)


def letter_confdrive():
    return settings_letter(KEY_LETTER_CONFDRIVE)


def log_config():
    log_dir = os.getenv('LOG_DIR')
    log_file = os.path.join(log_dir, 'mounter.log')

    os.makedirs(log_dir, exist_ok=True)
    app_log.remove()        # remove all previous log settings
    app_log.add(sys.stderr, format="{time:YYYY-MM-DD HH:mm:ss} {level: <7} {message}")
    app_log.add(log_file, format="{time:YYYY-MM-DD HH:mm:ss} {level: <7} {message}", rotation="1 MB", retention=1)


#@timeout(1)
def log_all_changed_settings(settings1: dict, settings2: dict):
    """ go through the supplied list of keys and log changed keys """

    cnt = 0
    app_log.debug(f"Changed keys:")

    for key in settings1.keys():        # go through all the keys, compare values
        v1 = settings1.get(key)
        v2 = settings2.get(key)

        if v1 != v2:  # compare values for key from both dicts - not equal? changed!
            app_log.debug(f"    {key}: {v1} -> {v2}")
            cnt += 1

    if not cnt:                         # nothing changed?
        app_log.debug(f"    (none)")


def setting_changed_on_keys(keys: list, settings1: dict, settings2: dict):
    """ go through the supplied list of keys, compare the dicts only on those keys """

    for key in keys:                                    # go through the supplied keys
        if settings1.get(key) != settings2.get(key):    # compare values for this key from both dicts - not equal? changed!
            return True

    # if got here, nothing was changed
    return False


#@timeout(1)
def load_old_settings():
    """ Load the old settings from .json file. We want to load the old settings so we can see which settings changed
    compared to the current ones. We cannot keep them in simple global variable because the settings loading
    will happen in threads and global vars from threads seem to get lost. """

    settings_json = text_from_file(FILE_OLD_SETTINGS)
    try:
        settinx = json.loads(settings_json)
    except TypeError:       # on fail to decode json, just return empty dict
        return {}

    return settinx


#@timeout(1)
def save_old_settings(settinx):
    """ save the old settings to .json file. """
    settings_json = json.dumps(settinx)
    text_to_file(settings_json, FILE_OLD_SETTINGS)
    return settinx


def load_one_setting(setting_name, default_value=None):
    """ load one setting from file and return it """
    path = os.path.join(os.getenv('SETTINGS_DIR'), setting_name)    # create full path

    if not os.path.exists(path) or not os.path.isfile(path):  # if it's not a file, skip it
        app_log.warning(f"failed to read file {path} - file does not exist")
        return default_value

    try:
        with open(path, "r") as file:  # read the file into value in dictionary
            value = file.readline()
            value = re.sub('[\n\r\t]', '', value)
            return value
    except UnicodeDecodeError:
        app_log.warning(f"failed to read file {path} - binary data in text file?")
    except Exception as ex:
        app_log.warning(f"failed to read file {path} with exception: {type(ex).__name__} - {str(ex)}")

    return default_value


#@timeout(1)
def load_current_settings():
    settinx = deepcopy(settings_default)       # fill settings with default values before loading

    os.makedirs(os.getenv('SETTINGS_DIR'), exist_ok=True)

    for f in os.listdir(os.getenv('SETTINGS_DIR')):         # go through the settings dir
        if f.startswith("."):                   # if it's a hidden file, skip it, because it might be nano .swp file
            continue

        settinx[f] = load_one_setting(f)        # load this setting from file

    return settinx


#@timeout(1)
def detect_settings_change(settings_current, settings_old):
    # find out if setting groups changed

    # check if ACSI translated drive letters changed
    changed_letters = setting_changed_on_keys(
        [KEY_LETTER_SHARED, KEY_LETTER_CONFDRIVE, KEY_LETTER_FIRST],
        settings_old, settings_current)

    # check if ACSI RAW IDs changed
    setting_keys_acsi_ids = [f'ACSI_DEVTYPE_{id_}' for id_ in range(8)]
    setting_keys_acsi_ids.append('HDDIMAGE')    # if hdd image changes, report this as ID change as we need to relink
    changed_ids = setting_changed_on_keys(setting_keys_acsi_ids, settings_old, settings_current)

    return changed_letters, changed_ids


def settings_load():
    """ load all the present settings from the settings dir """
    settinx = load_current_settings()
    settings_old = load_old_settings()          # load old settings from json
    log_all_changed_settings(settings_old, settinx)
    changed = detect_settings_change(settinx, settings_old)
    save_old_settings(settinx)                  # save the current settings to json file

    app_log.debug(f"settings_load - changed_letters: {changed[0]}, changed_ids: {changed[1]}")
    return changed


def setting_get_bool(setting_name):
    value = False
    value_raw = load_one_setting(setting_name)  # load this setting from file

    try:
        value = bool(int(value_raw))
    except Exception as exc:
        app_log.warning(f"for {setting_name} failed to convert {value} to bool: {str(exc)}")

    return value


def setting_get_int(setting_name):
    value = 0
    value_raw = load_one_setting(setting_name)  # load this setting from file

    try:
        value = int(value_raw)
    except Exception as exc:
        app_log.warning(f"failed to convert {value} to int: {str(exc)}")

    return value


def get_drives_bitno_from_settings():
    """ get letters from config, convert them to bit numbers """

    first = settings_letter_to_bitno(KEY_LETTER_FIRST)
    shared = settings_letter_to_bitno(KEY_LETTER_SHARED)     # network drive
    config = settings_letter_to_bitno(KEY_LETTER_CONFDRIVE)  # config drive

    return first, shared, config


def settings_letter(setting_name):
    """ get the letter from setting and return it """
    bitno = settings_letter_to_bitno(setting_name)
    letter = chr(65 + bitno)    # number to ascii char
    return letter


def settings_letter_to_bitno(setting_name):
    """ get setting by setting name, convert it to integer and then to drive bit number for Atari, e.g. 
    'a' is 0, 'b' is 1, 'p' is 15 """

    drive_letter = load_one_setting(setting_name, 'c')  # load this setting from file
    return letter_to_bitno(drive_letter)


def letter_to_bitno(drive_letter):
    """ convert drive letter to integer and then to drive bit number for Atari, e.g.
    'a' is 0, 'b' is 1, 'p' is 15 """

    letter = drive_letter.lower()        # get setting and convert it to lowercase
    letter = letter[:1]         # get only 0th character
    ascii_no = ord(letter)      # letter to ascii number ('c' -> 99)
    bitno = ascii_no - 97       # ascii number to bit number (99 -> 2)
    return bitno


def get_symlink_path_for_letter(letter):
    letter = letter.upper()
    path = os.path.join(os.getenv('MOUNT_DIR_TRANS'), letter)  # construct path where the drive letter should be mounted
    return path


def unlink_without_fail(path):
    # try to delete the symlink
    try:
        os.unlink(path)
        return True
    except FileNotFoundError:   # if it doesn't really exist, just ignore this exception (it's ok)
        pass
    except Exception as ex:     # if it existed (e.g. broken link) but failed to remove, log error
        app_log.warning(f'failed to unlink {path} - exception: {str(ex)}')

    return False


def readlink_without_fail(path):
    """ read the link, don't crash even if the file doesn't exist """

    source = None
    try:
        source = os.readlink(path)
    except FileNotFoundError:   # if it doesn't really exist, just ignore this exception (it's ok)
        pass
    except Exception as ex:     # log on other exceptions
        app_log.warning(f'readlink_without_fail: path: {path} - exception: {str(ex)}')

    return source


def get_usb_drive_letters(all_letters):
    """ Get all the drive letters which can be used for USB drives, if they are free.
    This list holds also the occupied letters.
    We create this list by skipping shared drive, config drive, zip drive, custom drives.

    :param all_letters: if true, return all usb drives - from C to P
                        if false, return usb drives from FIRST drive letter, e.g. from G to P
    """

    usb_drive_letters = []

    # get letters from config, convert them to bit numbers
    first, shared, config = get_drives_bitno_from_settings()

    # get custom user mount letters, so they could be skipped below
    from mount_user import get_user_custom_mounts_letters
    custom_letters = get_user_custom_mounts_letters()

    start = 2 if all_letters else first     # if all then start from drive #2 (C), else from first drive letter

    for i in range(start, 16):              # go through available drive letters - from 0 to 15
        if i in [shared, config]:           # skip these special drives
            continue

        letter = chr(65 + i)                # number to ascii char

        if letter in custom_letters:        # skip custom letters as they are not free
            continue

        usb_drive_letters.append(letter)    # this letter can be used for USB drives

    return usb_drive_letters


def unlink_drives_from_list(drive_letters):
    """ go through the list of drive letters and try to unlink them """
    app_log.debug(f'unlink_drives_from_list - will unlink these if they exist: {drive_letters}')

    for letter in drive_letters:                    # go through drive letters which can be used for USB drives
        path = get_symlink_path_for_letter(letter)  # construct path where the drive letter should be mounted
        good = unlink_without_fail(path)            # unlink if possible

        if good:
            app_log.debug(f'unlink_drives_from_list - unlinked {path}')


def unlink_everything_raw():
    """ go through all the ACSI ID paths and unlink them """
    from mount_usb_raw import get_symlink_path_for_id

    for id_ in range(8):
        symlink_path = get_symlink_path_for_id(id_)
        unlink_without_fail(symlink_path)            # unlink if possible


def unlink_everything_translated():
    """ go through all the possible drive letters and unlink them """
    all_drive_letters = [chr(65 + i) for i in range(2, 16)]         # generate drive letters from C to P
    unlink_drives_from_list(all_drive_letters)


def get_free_letters():
    """ Check which Atari drive letters are free and return them, also deletes broken links """
    letters_out = []
    sources_out = []

    drive_letters = get_usb_drive_letters(False)  # get all the possible USB drive letters (including the occupied ones)

    for letter in drive_letters:            # go through drive letters which can be used for USB drives

        # check if position at id_ is used or not
        path = get_symlink_path_for_letter(letter)    # construct path where the drive letter should be mounted

        if not os.path.exists(path):        # if mount point doesn't exist or symlink broken, it's free
            letters_out.append(letter)
            unlink_without_fail(path)       # try to delete the symlink
        else:                               # mount point exists
            if os.path.islink(path):        # and it's a symlink, read source and append it to list of sources
                source_path = os.readlink(path)
                sources_out.append(source_path)

    return letters_out, set(sources_out)


def umount_if_mounted(mount_dir, delete=False):
    """ check if this dir is mounted and if it is, then umount it """

    try:
        if not os.path.exists(mount_dir):
            app_log.info(f'umount_if_mounted: path {mount_dir} does not exists, not doing umount')
            return

        cmd = f'umount "{mount_dir}"'     # construct umount command
        os.system(cmd)                  # execute umount command

        if delete:                      # if should also delete folder
            os.rmdir(mount_dir)

        app_log.info(f'umount_if_mounted: umounted {mount_dir}')
    except Exception as exc:
        app_log.info(f'umount_if_mounted: failed to umount {mount_dir} : {str(exc)}')


def text_to_file(text, filename):
    # write text to file for later use
    try:
        with open(filename, 'wt') as f:
            f.write(text)
    except Exception as ex:
        app_log.warning(f"mount_shared: failed to write to {filename}: {str(ex)}")


def text_from_file(filename):
    # get text from file
    text = None

    if not os.path.exists(filename):    # no file like this exists? quit
        return None

    try:
        with open(filename, 'rt') as f:
            text = f.read()
            text = text.strip()         # remove whitespaces
    except Exception as ex:
        app_log.warning(f"mount_shared: failed to read {filename}: {str(ex)}")

    return text


def get_dir_usage(custom_letters, search_dir, name):
    """ turn folder name (drive letter) into what is this folder used for - config / shared / usb / zip drive """
    name_to_usage = {letter_shared(): "shared drive",
                     letter_confdrive(): "config drive"}

    for c_letter in custom_letters:                 # insert custom letters into name_to_usage
        name_to_usage[c_letter] = "custom drive"

    usage = name_to_usage.get(name, "USB drive")

    trans_path = fullpath = os.path.join(search_dir, name)       # create full path to this dir

    if os.path.islink(fullpath):                    # if this is a link, read source of the link
        fullpath = os.readlink(fullpath)

    desc_path = trans_path + ".desc"                # construct where the description of this disk should be stored
    desc_curr = text_from_file(desc_path)           # get current description
    desc_new = os.path.basename(fullpath)           # construct new description (currently just linux disk name)

    if desc_curr != desc_new:                       # the new description is different than current one? store new
        text_to_file(desc_new, desc_path)

    res = f"({usage})".ljust(20) + fullpath
    return res


def get_symlink_source(search_dir, name):
    fullpath = os.path.join(search_dir, name)       # create full path to this dir

    if os.path.islink(fullpath):                    # if this is a link, read source of the link
        fullpath = os.readlink(fullpath)

    res = f" ".ljust(20) + fullpath
    return res


def get_and_show_symlinks(search_dir, fun_on_each_found):
    dirs = []

    for name in os.listdir(search_dir):         # go through the dir
        dirs.append(name)                       # append to list of found

    if dirs:                                    # something was found?
        dirs = sorted(dirs)                     # sort results

        for one_dir in dirs:
            full_path = os.path.join(search_dir, one_dir)       # construct full path to dir

            if os.path.isfile(full_path) and not os.path.islink(full_path):     # skip files - they are not drives
                continue

            desc = '' if not fun_on_each_found else fun_on_each_found(search_dir, one_dir)  # get description if got function
            app_log.info(f" * {one_dir} {desc}")
    else:                                       # nothing was found
        app_log.info(f" (none)")


def show_symlinked_dirs():
    """ prints currently mounted / symlinked dirs """

    from mount_user import get_user_custom_mounts_letters
    custom_letters = get_user_custom_mounts_letters()           # fetch all the user custom letters

    # first show translated drives
    app_log.info(" ")
    app_log.info("list of current translated drives:")
    get_and_show_symlinks(os.getenv('MOUNT_DIR_TRANS'), partial(get_dir_usage, custom_letters))

    # then show RAW drives
    app_log.info(" ")
    app_log.info("list of current RAW drives:")
    get_and_show_symlinks(os.getenv('MOUNT_DIR_RAW'), get_symlink_source)

    app_log.info(" ")


def is_mountpoint_mounted(mountpoint):
    """ go through all the mounted disks and see if the specified mountpoint is present in the mounts or not """
    partitions = get_disk_partitions()

    for part in partitions:
        if part.mountpoint == mountpoint:       # mountpoint found, return True
            return True

    return False                                # mountpoint not found


def is_zip_mounted():
    """ ZIP file mounted? """
    return is_mountpoint_mounted(MOUNT_DIR_ZIP_FILE)        # return is_mounted, source


def is_shared_mounted():
    """ shared drive mounted? """
    is_mounted = is_mountpoint_mounted(MOUNT_DIR_SHARED)
    return is_mounted


def symlink_if_needed(lnk_source, lnk_dest):
    """ This function creates symlink from lnk_source to lnk_dest, but tries to do it smart, e.g.:
        - checks if the lnk_source really exists, doesn't try if it doesn't
        - if the symlink doesn't exist, it will symlink it
        - if the symlink does exist, it checks where the link points, and when the link is what it should be, then it
          doesn't symlink anything, so it links only in cases where the link is wrong
    """

    # check if the source (mount) dir exists, fail if it doesn't
    if not os.path.exists(lnk_source):
        app_log.warning(f"symlink_if_needed: mount_dir {lnk_source} does not exists!")
        return False

    symlink_it = False

    if not os.path.exists(lnk_dest):            # symlink dir doesn't exist - symlink it
        symlink_it = True
    else:                                       # symlink dir does exist - check if it's pointing to right source
        if os.path.islink(lnk_dest):            # if it's a symlink
            source_dir = os.readlink(lnk_dest)  # read symlink

            if source_dir != lnk_source:        # lnk_source of this link is not our mount dir
                symlink_it = True
        else:       # not a symlink - delete it, symlink it
            symlink_it = True

    # sym linking not needed? quit
    if not symlink_it:
        return False

    # should symlink now
    unlink_without_fail(lnk_dest)        # try to delete it

    good = False
    try:
        os.symlink(lnk_source, lnk_dest)  # symlink from mount path to symlink path
        app_log.debug(f'symlink_if_needed: symlinked {lnk_source} -> {lnk_dest}')
        good = True
    except Exception as ex:
        app_log.warning(f'symlink_if_needed: failed with: {type(ex).__name__} - {str(ex)}')

    return good


def other_instance_running():
    """ check if other instance of this app is running, return True if yes """
    pid_current = os.getpid()
    app_log.info(f'PID of this process: {pid_current}')

    pid_file = os.path.join(os.getenv('DATA_DIR'), 'mount.pid')
    os.makedirs(os.path.split(pid_file)[0], exist_ok=True)     # create dir for PID file if it doesn't exist

    # read PID from file and convert to int
    pid_from_file = -1
    try:
        pff = text_from_file(pid_file)
        pid_from_file = int(pff) if pff else -1
    except TypeError:       # we're expecting this on no text from file
        pass
    except Exception as ex:
        app_log.warning(f'other_instance_running: getting int PID from file failed: {type(ex).__name__} - {str(ex)}')

    # our and other PID match? no other instance
    if pid_current == pid_from_file:
        app_log.debug(f'other_instance_running: PID from file is ours, so other instance not running.')
        return False        # no other instance running

    # some other PID than ours was found in file
    if psutil.pid_exists(pid_from_file):
        app_log.warning(f'other_instance_running: Other mounter with PID {pid_from_file} is running!')
        return True         # other instance is running

    # other PID doesn't exist, no other instance running
    app_log.debug(f'other_instance_running: PID from file not running, so other instance not running')
    text_to_file(str(pid_current), pid_file)        # write our PID to file
    return False            # no other instance running


# This class serves as a result from get_disk_partitions(), so you can then access the paths in results with
# result.device and result.mountpoint, just like you would with psutil.disk_partitions()
class PartResult:
    def __init__(self, device, mountpoint):
        self.device = device
        self.mountpoint = mountpoint


def get_disk_partitions():
    # This function is a replacement for psutil.disk_partitions(True), which seemed like a perfect thing to use,
    # but it hangs on Raspberry Pi (on Raspbian) for like 12 seconds, so I've made this simpler version, which reads
    # from /proc/mounts . The 1st version used mount command in subprocess, but that doesn't work well with mount
    # points with spaces in it.
    # Example of such mount - my usb drive under xubuntu: '/media/miro/d-live 11.3.0 st i386'

    results = []

    with open("/proc/mounts") as file:      # read mounts file
        for line in file:
            pieces = line.split(' ', 2)     # split to device, mountpoint and rest

            if len(pieces) != 3:            # not 3 parts? probably bad line, skip it
                continue

            # in /proc/mounts the spaces inside device or path are represented with '\040',
            # so turn them back to spaces after splitting
            device = pieces[0].replace('\\040', ' ')
            mountpoint = pieces[1].replace('\\040', ' ')
            results.append(PartResult(device, mountpoint))

    return results


def get_root_fs_device():
    """
    Function gets device which is mounted to '/' on this linux box.
    It uses running mount command in subprocess, because:
        - psutil.disk_partitions() hangs on Raspbian 10
        - the content of /proc/mounts for mountpoint '/' on Raspbian shows device '/dev/root' and this
          doesn't exist
        - mount command shows this correctly as '/dev/sda2'
    """

    # try to get cached value from file
    dev_root_fs = text_from_file(FILE_ROOT_DEV)

    if dev_root_fs:         # if already got dev_root_fs figured out, just return it
        #app_log.debug(f'get_root_fs_device: returning cached root filesystem device: {dev_root_fs}')
        return dev_root_fs

    # run the mount command in subprocess, get output
    output = subprocess.Popen("mount", shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout.read()
    output = output.decode('utf-8')
    lines = output.splitlines()

    root_dev = '/dev/null'      # report this when no real device found

    for line in lines:
        chunks = line.split(' ', maxsplit=3)    # split line to 4 pieces - 'device', 'on', 'mountpoint', 'rest'
        device = chunks[0]
        mountpoint = chunks[2]

        if mountpoint == '/':                   # found root mount point? use this device
            root_dev = device
            break

    # if the device has numbers at the end, cut them off and leave only device name, e.g. /dev/sda2 -> /dev/sda
    while root_dev[-1].isnumeric():             # last char is a number?
        root_dev = root_dev[:-1]                # remove last char

    # app_log.debug(f'get_root_fs_device: root filesystem device: {root_dev}')
    text_to_file(root_dev, FILE_ROOT_DEV)       # cache this value to file
    return root_dev


def send_to_socket(sock_path, item):
    """ send an item to core """
    try:
        app_log.debug(f"sending {item} to {sock_path}")
        json_item = json.dumps(item)   # dict to json

        sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        sock.connect(sock_path)
        sock.send(json_item.encode('utf-8'))
        sock.close()
    except Exception as ex:
        app_log.debug(f"failed to send {item} - {str(ex)}")


def send_to_core(item):
    """ send an item to core """
    send_to_socket(os.getenv('CORE_SOCK_PATH'), item)


def send_to_taskq(item):
    """ send an item to task queue """
    send_to_socket(os.getenv('TASKQ_SOCK_PATH'), item)


def send_to_mounter(item):
    """ send an item to mounter """
    send_to_socket(os.getenv('MOUNT_SOCK_PATH'), item)


def trigger_reload_translated():
    """ when translated disks change, trigger disks reload in core """
    app_log.debug(f"Will now trigger reload of translated drives in core!")
    item = {'module': 'disks', 'action': 'reload_trans'}
    send_to_core(item)


def trigger_reload_raw():
    """ when raw disks change, trigger disks reload in core """
    app_log.debug(f"Will now trigger reload of raw drives in core!")
    item = {'module': 'disks', 'action': 'reload_raw'}
    send_to_core(item)


def copy_and_symlink_config_dir():
    """ create a copy of configdir, then symlink it to right place """
    if not os.path.exists(os.getenv('CONFIG_PATH_SOURCE')):
        app_log.warning(f"Config drive origin folder doesn't exist! ( {os.getenv('CONFIG_PATH_SOURCE')} )")
        return

    symlinked = False
    try:
        os.makedirs(os.getenv('CONFIG_PATH_COPY'), exist_ok=True)                    # create dir for copy
        os.system(f"cp -r {os.getenv('CONFIG_PATH_SOURCE')}/* {os.getenv('CONFIG_PATH_COPY')}")   # copy original config dir to copy dir

        symlink_path = get_symlink_path_for_letter(letter_confdrive())

        symlinked = symlink_if_needed(os.getenv('CONFIG_PATH_COPY'), symlink_path)  # create symlink, but only if needed
        app_log.info(f"Config drive was symlinked to: {symlink_path}")
    except Exception as ex:
        app_log.warning(f'copy_and_symlink_config_dir: failed with: {type(ex).__name__} - {str(ex)}')

    if symlinked:           # if config drive was symlinked, trigger core reload of translated disks
        trigger_reload_translated()
