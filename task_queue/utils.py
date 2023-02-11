import os
import re
import json
import socket
from loguru import logger as app_log
import logging
from dotenv import load_dotenv
import psutil
import subprocess


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


def can_write_to_storage(path):
    """ check if can write to storage path """
    test_file = os.path.join(path, "test_file.txt")

    try:        # first remove the test file if it exists
        os.remove(test_file)
    except:     # don't fail if failed to remove file - might not exist at this time
        pass

    good = False

    try:        # create file and write to file
        with open(test_file, 'wt') as out:
            out.write("whatever")

        good = True
    except:     # this error is expected
        pass

    try:        # remove the test file if it exists
        os.remove(test_file)
    except:     # don't fail if failed to remove file - might not exist at this time
        pass

    return good


def get_storage_path():
    """ Find the storage path and return it. Use cached value if possible. """
    global last_storage_path

    last_storage_exists = last_storage_path is not None and os.path.exists(last_storage_path)  # path still valid?

    if last_storage_exists:  # while the last storage exists, keep returning it
        return last_storage_path

    storage_path = None

    # does this symlink exist?
    download_storage_dir = os.getenv('DOWNLOAD_STORAGE_DIR')
    if os.path.exists(download_storage_dir) and os.path.islink(download_storage_dir):
        storage_path = os.readlink(download_storage_dir)    # read the symlink

        if not os.path.exists(storage_path):                # symlink source doesn't exist? reset path to None
            storage_path = None

    if storage_path:    # if storage path was found, append subdir to it
        storage_path = os.path.join(storage_path, "fdd_imgs")
        os.makedirs(storage_path, exist_ok=True)

    # store the found storage path and return it
    last_storage_path = storage_path
    return storage_path


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


def log_config():
    log_dir = os.getenv('LOG_DIR')
    os.makedirs(log_dir, exist_ok=True)
    log_file = os.path.join(log_dir, 'taskq.log')

    app_log.add(log_file, rotation="1 MB", retention=1)


def text_to_file(text, filename):
    # write text to file for later use
    try:
        with open(filename, 'wt') as f:
            f.write(text)
    except Exception as ex:
        app_log.warning(logging.WARNING, f"failed to write to {filename}: {str(ex)}")


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


def other_instance_running():
    """ check if other instance of this app is running, return True if yes """
    pid_current = os.getpid()
    app_log.info(f'PID of this process: {pid_current}')

    pid_file = os.path.join(os.getenv('DATA_DIR'), 'taskq.pid')
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


def unlink_without_fail(path):
    # try to delete the file
    try:
        os.unlink(path)
        return True
    except FileNotFoundError:   # if it doesn't really exist, just ignore this exception (it's ok)
        pass
    except Exception as ex:     # if it existed (e.g. broken link) but failed to remove, log error
        app.logger.warning(f'failed to unlink {path} - exception: {str(ex)}')

    return False


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
