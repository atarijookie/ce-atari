import copy
import os
import re
import urwid
from setproctitle import setproctitle
import logging
from logging.handlers import RotatingFileHandler
from urwid_helpers import create_edit_one, create_my_button, create_header_footer, create_edit, MyRadioButton, \
    MyCheckBox, dialog
import shared


app_log = logging.getLogger()

settings_path = "/ce/settings"          # path to settings dir
settings_default = {'DRIVELETTER_FIRST': 'C', 'DRIVELETTER_SHARED': 'P', 'DRIVELETTER_CONFDRIVE': 'O',
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


def setting_get_bool(setting_name):
    value = False
    value_raw = setting_get_merged(setting_name)

    try:
        value = bool(int(value_raw))
    except Exception as exc:
        app_log.warning(f"failed to convert {value} to bool: {str(exc)}")

    return value


def setting_get_merged(setting_name):
    """ function gets either new value, or stored value """

    if setting_name in shared.settings_changed:         # got new setting value in changes settings? return that
        return shared.settings_changed[setting_name]

    return shared.settings.get(setting_name)            # get value from stored settings


def settings_load():
    """ load all the present settings from the settings dir """

    shared.settings_changed = {}                # no changes settings yet

    shared.settings = copy.deepcopy(settings_default)  # fill settings with default values before loading

    for f in os.listdir(settings_path):         # go through the settings dir
        path = os.path.join(settings_path, f)   # create full path

        if not os.path.isfile(path):            # if it's not a file, skip it
            continue

        with open(path, "r") as file:           # read the file into value in dictionary
            value = file.readline()
            value = re.sub('[\n\r\t]', '', value)
            shared.settings[f] = value
            #app_log.debug(f"settings_load: settings[{f}] = {value}")


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
