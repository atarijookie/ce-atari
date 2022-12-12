import copy
import os
import re
import urwid
import logging
import shared


app_log = logging.getLogger()

settings_path = "/ce/settings"          # path to settings dir
settings_default = {'DRIVELETTER_FIRST': 'C', 'DRIVELETTER_CONFDRIVE': 'O', 'DRIVELETTER_SHARED': 'N', 'DRIVELETTER_ZIP': 'P',
                    'MOUNT_RAW_NOT_TRANS': 0, 'SHARED_ENABLED': 0, 'SHARED_NFS_NOT_SAMBA': 0, 'FLOPPYCONF_ENABLED': 1,
                    'FLOPPYCONF_DRIVEID': 0, 'FLOPPYCONF_WRITEPROTECTED': 0, 'FLOPPYCONF_SOUND_ENABLED': 1,
                    'ACSI_DEVTYPE_0': 0, 'ACSI_DEVTYPE_1': 1, 'ACSI_DEVTYPE_2': 0, 'ACSI_DEVTYPE_3': 0,
                    'ACSI_DEVTYPE_4': 0, 'ACSI_DEVTYPE_5': 0, 'ACSI_DEVTYPE_6': 0, 'ACSI_DEVTYPE_7': 0,
                    'KEYBOARD_KEYS_JOY0': 'A%S%D%W%LSHIFT', 'KEYBOARD_KEYS_JOY1': 'LEFT%DOWN%RIGHT%UP%RSHIFT'}


def setting_get_str(setting_name):
    value_raw = setting_get_merged(setting_name)        # get value from settings

    if not value_raw:       # if it's None, replace with empty string
        return ''

    return value_raw        # not None, return as is


def setting_get_int(setting_name):
    value = 0

    try:
        value_raw = setting_get_merged(setting_name)
        value = int(value_raw)
    except Exception as exc:
        app_log.warning(f"failed to convert {value_raw} to int: {str(exc)}")

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
    path = os.path.join(settings_path, setting_name)  # create full path

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

    shared.settings = copy.deepcopy(settings_default)  # fill settings with default values before loading

    for f in os.listdir(settings_path):         # go through the settings dir
        shared.settings[f] = setting_load_one(f)


def settings_save():
    """ save only changed settings to settings dir """
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


FILE_STATUS = '/tmp/UPDATE_STATUS'
FILE_PENDING_YES = '/tmp/UPDATE_PENDING_YES'
FILE_PENDING_NO = '/tmp/UPDATE_PENDING_NO'


def delete_update_files():
    """ delete all update chceck files """
    for path in [FILE_STATUS, FILE_PENDING_YES, FILE_PENDING_NO]:
        delete_file(path)


def text_to_file(text, filename):
    # write text to file for later use
    try:
        with open(filename, 'wt') as f:
            f.write(text)
    except Exception as ex:
        app_log.warning(logging.WARNING, f"mount_shared: failed to write to {filename}: {str(ex)}")


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
        app_log.warning(logging.WARNING, f"mount_shared: failed to read {filename}: {str(ex)}")

    return text
