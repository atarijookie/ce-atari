from copy import deepcopy
import os
import re
import logging
from logging.handlers import RotatingFileHandler

app_log = logging.getLogger()

SETTINGS_PATH = "/ce/settings"          # path to settings dir

settings_default = {'DRIVELETTER_FIRST': 'C', 'MOUNT_RAW_NOT_TRANS': 0, 'SHARED_ENABLED': 0, 'SHARED_NFS_NOT_SAMBA': 0,
                    'ACSI_DEVTYPE_0': 0, 'ACSI_DEVTYPE_1': 1, 'ACSI_DEVTYPE_2': 0, 'ACSI_DEVTYPE_3': 0,
                    'ACSI_DEVTYPE_4': 0, 'ACSI_DEVTYPE_5': 0, 'ACSI_DEVTYPE_6': 0, 'ACSI_DEVTYPE_7': 0}

settings = {}

LOG_DIR = '/var/log/ce/'
DATA_DIR = '/var/run/ce/'
MOUNT_LOG_FILE = os.path.join(LOG_DIR, 'mount.log')
MOUNT_COMMANDS_DIR = os.path.join(DATA_DIR, 'cmds')
MOUNT_DIR_RAW = os.path.join(DATA_DIR, 'raw')
MOUNT_DIR_TRANS = os.path.join(DATA_DIR, 'trans')
LETTER_SHARED = 'N'                     # Network drive on N
LETTER_CONFIG = 'O'                     # cOnfig drive on O
LETTER_ZIP = 'P'                        # ziP file drive on P

DEV_DISK_DIR = '/dev/disk/by-path'


def log_config():
    log_formatter = logging.Formatter('%(asctime)s %(levelname)s %(funcName)s(%(lineno)d) %(message)s')

    my_handler = RotatingFileHandler(f'{LOG_DIR}/ce_mounter.log', mode='a', maxBytes=1024 * 1024, backupCount=1)
    my_handler.setFormatter(log_formatter)
    my_handler.setLevel(logging.DEBUG)

    app_log.setLevel(logging.DEBUG)
    app_log.addHandler(my_handler)


def print_and_log(loglevel, message):
    """ print to console and store to log """
    print(message)
    app_log.log(loglevel, message)


def setting_changed_on_keys(keys: list, settings1: dict, settings2: dict):
    """ go through the supplied list of keys, compare the dicts only on those keys """

    for key in keys:                                    # go through the supplied keys
        if settings1.get(key) != settings2.get(key):    # compare values for this key from both dicts - not equal? changed!
            return True

    # if got here, nothing was changed
    return False


def settings_load():
    """ load all the present settings from the settings dir """

    global settings
    settings_old = deepcopy(settings)           # make copy of previous settings before loading new ones
    settings = deepcopy(settings_default)       # fill settings with default values before loading

    os.makedirs(SETTINGS_PATH, exist_ok=True)

    for f in os.listdir(SETTINGS_PATH):         # go through the settings dir
        path = os.path.join(SETTINGS_PATH, f)   # create full path

        if not os.path.isfile(path):            # if it's not a file, skip it
            continue

        with open(path, "r") as file:           # read the file into value in dictionary
            value = file.readline()
            value = re.sub('[\n\r\t]', '', value)
            settings[f] = value

    # find out if setting groups changed
    changed_usb = setting_changed_on_keys(
        ['DRIVELETTER_FIRST', 'MOUNT_RAW_NOT_TRANS', 'ACSI_DEVTYPE_0', 'ACSI_DEVTYPE_1', 'ACSI_DEVTYPE_2',
         'ACSI_DEVTYPE_3', 'ACSI_DEVTYPE_4', 'ACSI_DEVTYPE_5', 'ACSI_DEVTYPE_6', 'ACSI_DEVTYPE_7'],
        settings_old, settings)

    changed_shared = setting_changed_on_keys(
        ['SHARED_ENABLED', 'SHARED_NFS_NOT_SAMBA', 'SHARED_ADDRESS', 'SHARED_PATH', 'SHARED_USERNAME',
         'SHARED_PASSWORD'],
        settings_old, settings)

    return changed_usb, changed_shared


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


def umount_if_mounted(mount_dir, delete=False):
    """ check if this dir is mounted and if it is, then umount it """

    try:
        if not os.path.exists(mount_dir):
            print_and_log(logging.INFO, f'umount_if_mounted: path {mount_dir} does not exists, not doing umount')
            return

        cmd = f'umount {mount_dir}'     # construct umount command
        os.system(cmd)                  # execute umount command

        if delete:                      # if should also delete folder
            os.rmdir(mount_dir)

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
