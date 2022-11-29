from copy import deepcopy
import os
import re
import logging
import json
from logging.handlers import RotatingFileHandler
from wrapt_timeout_decorator import timeout

app_log = logging.getLogger()

SETTINGS_PATH = "/ce/settings"          # path to settings dir

KEY_LETTER_FIRST = 'DRIVELETTER_FIRST'
KEY_LETTER_CONFDRIVE = 'DRIVELETTER_CONFDRIVE'
KEY_LETTER_SHARED = 'DRIVELETTER_SHARED'
KEY_LETTER_ZIP = 'DRIVELETTER_ZIP'

DEV_TYPE_OFF = 0
DEV_TYPE_SD = 1
DEV_TYPE_RAW = 2
DEV_TYPE_CEDD = 3

settings_default = {KEY_LETTER_FIRST: 'C', KEY_LETTER_CONFDRIVE: 'O', KEY_LETTER_SHARED: 'N',
                    KEY_LETTER_ZIP: 'P',
                    'MOUNT_RAW_NOT_TRANS': 0, 'SHARED_ENABLED': 0, 'SHARED_NFS_NOT_SAMBA': 0,
                    'ACSI_DEVTYPE_0': DEV_TYPE_OFF, 'ACSI_DEVTYPE_1': DEV_TYPE_CEDD, 'ACSI_DEVTYPE_2': DEV_TYPE_OFF,
                    'ACSI_DEVTYPE_3': DEV_TYPE_OFF, 'ACSI_DEVTYPE_4': DEV_TYPE_OFF, 'ACSI_DEVTYPE_5': DEV_TYPE_OFF,
                    'ACSI_DEVTYPE_6': DEV_TYPE_OFF, 'ACSI_DEVTYPE_7': DEV_TYPE_OFF}

LOG_DIR = '/var/log/ce/'
DATA_DIR = '/var/run/ce/'
MOUNT_LOG_FILE = os.path.join(LOG_DIR, 'mount.log')
MOUNT_COMMANDS_DIR = os.path.join(DATA_DIR, 'cmds')
MOUNT_DIR_RAW = os.path.join(DATA_DIR, 'raw')
MOUNT_DIR_TRANS = os.path.join(DATA_DIR, 'trans')
MOUNT_SHARED_CMD_LAST = os.path.join(DATA_DIR, 'mount_shared_cmd_last')
CONFIG_PATH_SOURCE = "/ce/app/configdrive"
CONFIG_PATH_COPY = os.path.join(DATA_DIR, 'configdrive')

FILE_MOUNT_CMD_SAMBA = os.path.join(SETTINGS_PATH, 'mount_cmd_samba.txt')
FILE_MOUNT_CMD_NFS = os.path.join(SETTINGS_PATH, 'mount_cmd_nfs.txt')

FILE_MOUNT_USER = os.path.join(SETTINGS_PATH, 'mount_user.txt')         # user custom mounts in settings dir
FILE_HDDIMAGE_RESOLVED = os.path.join(DATA_DIR, 'HDDIMAGE_RESOLVED')    # where the resolved HDDIMAGE will end up
FILE_OLD_SETTINGS = os.path.join(DATA_DIR, 'settings_old.json')         # where the old settings in json are

DEV_DISK_DIR = '/dev/disk/by-path'


def letter_shared():
    return settings_letter(KEY_LETTER_SHARED)


def letter_confdrive():
    return settings_letter(KEY_LETTER_CONFDRIVE)


def letter_zip():
    return settings_letter(KEY_LETTER_ZIP)


def log_config():
    os.makedirs(LOG_DIR, exist_ok=True)

    log_formatter = logging.Formatter('%(asctime)s %(levelname)s %(funcName)s(%(lineno)d) %(message)s')

    my_handler = RotatingFileHandler(f'{LOG_DIR}/ce_mounter.log', mode='a', maxBytes=1024 * 1024, backupCount=1)
    my_handler.setFormatter(log_formatter)
    my_handler.setLevel(logging.DEBUG)

    app_log.setLevel(logging.DEBUG)
    app_log.addHandler(my_handler)


def print_and_log(loglevel, message):
    """ print to console and store to log """
    if loglevel in [logging.WARNING, logging.ERROR]:        # highlight messages with issues
        message = f"!!! {message} !!!"

    loglevel_string = logging.getLevelName(loglevel).ljust(8)
    message = loglevel_string + " " + message               # add log level to message

    print(message)
    app_log.log(loglevel, message)


@timeout(1)
def log_all_changed_settings(settings1: dict, settings2: dict):
    """ go through the supplied list of keys and log changed keys """

    cnt = 0
    print_and_log(logging.DEBUG, f"Changed keys:")

    for key in settings1.keys():        # go through all the keys, compare values
        v1 = settings1.get(key)
        v2 = settings2.get(key)

        if v1 != v2:  # compare values for key from both dicts - not equal? changed!
            print_and_log(logging.DEBUG, f"    {key}: {v1} -> {v2}")
            cnt += 1

    if not cnt:                         # nothing changed?
        print_and_log(logging.DEBUG, f"    (none)")


def setting_changed_on_keys(keys: list, settings1: dict, settings2: dict):
    """ go through the supplied list of keys, compare the dicts only on those keys """

    for key in keys:                                    # go through the supplied keys
        if settings1.get(key) != settings2.get(key):    # compare values for this key from both dicts - not equal? changed!
            return True

    # if got here, nothing was changed
    return False


@timeout(1)
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


@timeout(1)
def save_old_settings(settinx):
    """ save the old settings to .json file. """
    settings_json = json.dumps(settinx)
    text_to_file(settings_json, FILE_OLD_SETTINGS)
    return settinx


def load_one_setting(setting_name, default_value=None):
    """ load one setting from file and return it """
    path = os.path.join(SETTINGS_PATH, setting_name)    # create full path

    if not os.path.isfile(path):  # if it's not a file, skip it
        return default_value

    try:
        with open(path, "r") as file:  # read the file into value in dictionary
            value = file.readline()
            value = re.sub('[\n\r\t]', '', value)
            return value
    except UnicodeDecodeError:
        print_and_log(logging.WARNING, f"failed to read file {path} - binary data in text file?")
    except Exception as ex:
        print_and_log(logging.WARNING, f"failed to read file {path} with exception: {type(ex).__name__} - {str(ex)}")

    return default_value


@timeout(1)
def load_current_settings():
    settinx = deepcopy(settings_default)       # fill settings with default values before loading

    os.makedirs(SETTINGS_PATH, exist_ok=True)

    for f in os.listdir(SETTINGS_PATH):         # go through the settings dir
        if f.startswith("."):                   # if it's a hidden file, skip it, because it might be nano .swp file
            continue

        settinx[f] = load_one_setting(f)        # load this setting from file

    return settinx


@timeout(1)
def detect_settings_change(settings_current, settings_old):
    # find out if setting groups changed

    # check if ACSI translated drive letters changed
    changed_letters = setting_changed_on_keys(
        [KEY_LETTER_SHARED, KEY_LETTER_CONFDRIVE, KEY_LETTER_ZIP, KEY_LETTER_FIRST],
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

    print_and_log(logging.DEBUG, f"settings_load - changed_letters: {changed[0]}, changed_ids: {changed[1]}")
    return changed


def setting_get_bool(setting_name):
    value = False
    value_raw = load_one_setting(setting_name)  # load this setting from file

    try:
        value = bool(int(value_raw))
    except Exception as exc:
        print_and_log(logging.WARNING, f"failed to convert {value} to bool: {str(exc)}")

    return value


def setting_get_int(setting_name):
    value = 0
    value_raw = load_one_setting(setting_name)  # load this setting from file

    try:
        value = int(value_raw)
    except Exception as exc:
        print_and_log(logging.WARNING, f"failed to convert {value} to int: {str(exc)}")

    return value


def get_drives_bitno_from_settings():
    """ get letters from config, convert them to bit numbers """

    first = settings_letter_to_bitno(KEY_LETTER_FIRST)
    shared = settings_letter_to_bitno(KEY_LETTER_SHARED)     # network drive
    config = settings_letter_to_bitno(KEY_LETTER_CONFDRIVE)  # config drive
    zip_ = settings_letter_to_bitno(KEY_LETTER_ZIP)          # ZIP file drive

    return first, shared, config, zip_


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
    path = os.path.join(MOUNT_DIR_TRANS, letter)  # construct path where the drive letter should be mounted
    return path


def unlink_without_fail(path):
    # try to delete the symlink
    try:
        os.unlink(path)
        return True
    except FileNotFoundError:   # if it doesn't really exist, just ignore this exception (it's ok)
        pass
    except Exception as ex:     # if it existed (e.g. broken link) but failed to remove, log error
        print_and_log(logging.WARNING, f'failed to unlink {path} - exception: {str(ex)}')

    return False


def get_usb_drive_letters(all_letters):
    """ Get all the drive letters which can be used for USB drives, if they are free.
    This list holds also the occupied letters.
    We create this list by skipping shared drive, config drive, zip drive, custom drives.

    :param all_letters: if true, return all usb drives - from C to P
                        if false, return usb drives from FIRST drive letter, e.g. from G to P
    """

    usb_drive_letters = []

    # get letters from config, convert them to bit numbers
    first, shared, config, zip_ = get_drives_bitno_from_settings()

    # get custom user mount letters, so they could be skipped below
    from mount_user import get_user_custom_mounts_letters
    custom_letters = get_user_custom_mounts_letters()

    start = 2 if all_letters else first     # if all then start from drive #2 (C), else from first drive letter

    for i in range(start, 16):              # go through available drive letters - from 0 to 15
        if i in [shared, config, zip_]:     # skip these special drives
            continue

        letter = chr(65 + i)                # number to ascii char

        if letter in custom_letters:        # skip custom letters as they are not free
            continue

        usb_drive_letters.append(letter)    # this letter can be used for USB drives

    return usb_drive_letters


def unlink_drives_from_list(drive_letters):
    """ go through the list of drive letters and try to unlink them """
    print_and_log(logging.DEBUG, f'unlink_drives_from_list - will unlink these if they exist: {drive_letters}')

    for letter in drive_letters:                    # go through drive letters which can be used for USB drives
        path = get_symlink_path_for_letter(letter)  # construct path where the drive letter should be mounted
        good = unlink_without_fail(path)            # unlink if possible

        if good:
            print_and_log(logging.DEBUG, f'unlink_drives_from_list - unlinked {path}')


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


def unlink_all_usb_drives():
    """ go through all the possible USB drive letters and unlink them """
    drive_letters = get_usb_drive_letters(True)  # get all the possible USB drive letters (including the occupied ones)
    unlink_drives_from_list(drive_letters)


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


def delete_files(files):
    for file in files:
        unlink_without_fail(file)


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


def text_to_file(text, filename):
    # write text to file for later use
    try:
        with open(filename, 'wt') as f:
            f.write(text)
    except Exception as ex:
        print_and_log(logging.WARNING, f"mount_shared: failed to write to {filename}: {str(ex)}")


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
        print_and_log(logging.WARNING, f"mount_shared: failed to read {filename}: {str(ex)}")

    return text
