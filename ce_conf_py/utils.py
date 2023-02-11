import copy
import os
import re
import urwid
from loguru import logger as app_log
import psutil
import shared
from dotenv import load_dotenv
import subprocess

settings_default = {'DRIVELETTER_FIRST': 'C', 'DRIVELETTER_CONFDRIVE': 'O', 'DRIVELETTER_SHARED': 'N',
                    'MOUNT_RAW_NOT_TRANS': 0, 'SHARED_ENABLED': 0, 'SHARED_NFS_NOT_SAMBA': 0, 'FLOPPYCONF_ENABLED': 1,
                    'USE_ZIP_DIR': 1,
                    'FLOPPYCONF_DRIVEID': 0, 'FLOPPYCONF_WRITEPROTECTED': 0, 'FLOPPYCONF_SOUND_ENABLED': 1,
                    'ACSI_DEVTYPE_0': 0, 'ACSI_DEVTYPE_1': 1, 'ACSI_DEVTYPE_2': 0, 'ACSI_DEVTYPE_3': 0,
                    'ACSI_DEVTYPE_4': 0, 'ACSI_DEVTYPE_5': 0, 'ACSI_DEVTYPE_6': 0, 'ACSI_DEVTYPE_7': 0,
                    'KEYBOARD_KEYS_JOY0': 'A%S%D%W%LSHIFT', 'KEYBOARD_KEYS_JOY1': 'LEFT%DOWN%RIGHT%UP%RSHIFT',
                    'TIME_SET': 1, 'TIME_UTC_OFFSET': 0, 'TIME_NTP_SERVER': 'pool.ntp.org',
                    'SCREENCAST_FRAMESKIP': 20, 'SCREEN_RESOLUTION': 1
                    }


def load_dotenv_config():
    """ Try to load the dotenv configuration file.
    First try to see if there's an override path for this config file specified in the env variables.
    Then try the normal installation path for dotenv on ce: /ce/services/.env
    If that fails, try to find and use local dotenv file used during development - .env in your local dir
    """

    # First try to see if there's an override path for this config file specified in the env variables.
    path = os.environ.get('CE_DOTENV_PATH')

    if path and os.path.exists(path):       # path in env found and it really exists, use it
        load_dotenv(dotenv_path=path)
        return

    # Then try the normal installation path for dotenv on ce: /ce/services/.env
    ce_dot_env_file = '/ce/services/.env'
    if os.path.exists(ce_dot_env_file):
        load_dotenv(dotenv_path=ce_dot_env_file)
        return

    # If that fails, try to find and use local dotenv file used during development - .env in your local dir
    load_dotenv()


def setting_get_str(setting_name):
    value_raw = setting_get_merged(setting_name)        # get value from settings

    if value_raw is None:   # if it's None, replace with empty string
        return ''

    return value_raw        # not None, return as is


def setting_get_int(setting_name):
    value = 0

    try:
        value_raw = setting_get_merged(setting_name)

        if value_raw is None:       # value not present? just use default value
            return value

        value = int(value_raw)
    except Exception as exc:
        app_log.warning(f"for {setting_name} failed to convert {value_raw} to int: {str(exc)}")

    return value


def setting_get_bool(setting_name):
    value = False

    try:
        value_raw = setting_get_int(setting_name)
        value = bool(value_raw)
    except Exception as exc:
        app_log.warning(f"failed to convert {value_raw} to bool: {str(exc)}")

    return value


def setting_get_merged(setting_name):
    """ function gets either new value, or stored value """

    if setting_name in shared.settings_changed:         # got new setting value in changes settings? return that
        return shared.settings_changed[setting_name]

    return shared.settings.get(setting_name)            # get value from stored settings


def setting_load_one(setting_name, default_value=None):
    settings_path = os.getenv('SETTINGS_DIR')           # path to settings dir
    path = os.path.join(settings_path, setting_name)    # create full path

    if not os.path.isfile(path):  # if it's not a file, skip it
        return default_value

    try:
        with open(path, "r") as file:  # read the file into value in dictionary
            value = file.readline()
            value = re.sub('[\n\r\t]', '', value)
            return value
    except Exception as ex:
        app_log.debug(f"setting_load_one: failed to load {setting_name} - exception: {str(ex)}")

    return default_value


def settings_load():
    """ load all the present settings from the settings dir """

    shared.settings_changed = {}                # no changes settings yet

    shared.settings = copy.deepcopy(settings_default)   # fill settings with default values before loading
    settings_path = os.getenv('SETTINGS_DIR')           # path to settings dir

    for f in os.listdir(settings_path):         # go through the settings dir
        shared.settings[f] = setting_load_one(f)


def settings_save():
    """ save only changed settings to settings dir """
    settings_path = os.getenv('SETTINGS_DIR')           # path to settings dir

    for key, value in shared.settings_changed.items():     # get all the settings that have changed
        path = os.path.join(settings_path, key)     # create full path

        with open(path, "w") as file:               # write to that file
            file.write(str(value))
            app_log.debug(f"settings_save: {key} -> {value}")


def on_cancel(button):
    shared.settings_changed = {}    # no settings have been changed
    back_to_main_menu(None)         # return to main menu


def back_to_main_menu(button):
    """ when we should return back to main menu """
    from main import create_main_menu
    shared.main.original_widget = urwid.Padding(create_main_menu(), left=2, right=2)


def on_option_changed(button, state, data):
    """ on option changed """
    if not state:                           # called on the radiobutton, which is now off? skip it
        return

    key = data['id']                        # get key
    value = data['value']
    shared.settings_changed[key] = value           # store value
    app_log.debug(f"on_option_changed: {key} -> {value}")


def on_checkbox_changed(setting_name, state):
    value = 1 if state else 0
    app_log.debug(f"on_checkbox_changed - setting_name: {setting_name} -> value: {value}")
    shared.settings_changed[setting_name] = value


def on_editline_changed(widget, text, data):
    id_ = data.get('id')                    # get id

    if id_:         # if got it, store text
        shared.settings_changed[id_] = text


def delete_file(path):
    """ function to delete file and not die on exception if it fails """
    if os.path.exists(path):
        try:
            os.remove(path)
        except Exception as ex:
            app_log.warning('Could not delete file {}', path)


def delete_update_files():
    """ delete all update chceck files """
    for path in [os.getenv('FILE_UPDATE_STATUS'), os.getenv('FILE_UPDATE_PENDING_YES'), os.getenv('FILE_UPDATE_PENDING_NO')]:
        delete_file(path)


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


def log_config():
    log_dir = os.getenv('LOG_DIR')
    log_file = os.path.join(log_dir, 'ce_conf_py.log')

    os.makedirs(log_dir, exist_ok=True)
    app_log.remove()        # remove all previous log settings
    app_log.add(log_file, rotation="1 MB", retention=1)


def other_instance_running():
    """ check if other instance of this app is running, return True if yes """
    pid_current = os.getpid()
    app_log.info(f'PID of this process: {pid_current}')

    pid_file = os.path.join(os.getenv('DATA_DIR'), 'ce_config.pid')
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


def system_custom(command_str, to_log=True, shell=False):
    """ This is a replacement for os.system() from which it's harder to get the output
        and also for direct calling of subprocess.run(), where you should pass in list instead of string.

        @param command_str: command with arguments as string
        @param to_log: if true, log the output of the command
        @param shell: if true, subprocess.run() runs the command with shell binary (== heavier than shel=False)
    """

    # subprocess.run() can accept command with arguments as:
    # string - if shell=True
    # list - if shell=False
    # The problem is that when you run it with shell=True, it doesn't lunch the executable directly, but it first
    # starts the shell binary and then the executable, so whenever we can, we should run it without shell, thus
    # with list of arguments instead of strings. But writing the command as list of strings instead of single string
    # is annoying, so instead this function takes in a string and splits it to list of strings.
    # But this fails in some cases, e.g. when supplying "" as empty ip address to nmcli, so for that case we allow
    # to run that command with shell=True.

    if '"' in command_str and not shell:    # add this warning for future developers
        app_log.warning('Hey! Your command string has " character in it, if the command is failing, call system_custom() with shell=True')

    if shell:       # run with shell - heavier, but sometimes necessary to make it work
        result = subprocess.run(command_str, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
    else:           # run without shell - lighter, but fails sometimes
        command_list = command_str.split(' ')  # split command from string to list
        result = subprocess.run(command_list, stdout=subprocess.PIPE, stderr=subprocess.PIPE)   # run the command list

    stdout = result.stdout.decode('utf-8')                          # get output as string
    stderr = result.stderr.decode('utf-8')

    if to_log:
        app_log.debug(f'command   : {command_str}')
        app_log.debug(f'returncode: {result.returncode}')
        app_log.debug(f'cmd stdout: {stdout}')
        app_log.debug(f'cmd stderr: {stderr}')

    return stdout, result.returncode
