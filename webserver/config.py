import os
import re
import math
import json
import stat
import logging
from os import listdir
from os.path import isfile, isdir, join

from flask import Blueprint, request, current_app as app, abort
from utils import get_setting, set_setting, is_blockdev, send_to_core_hdd

config = Blueprint('config', __name__)
app_log = logging.getLogger()


@config.route('/get_ids', methods=['GET'])
def get_ids():
    """ get paths for ACSI IDs """
    resp = {'paths': [], 'dev_types': []}

    # get path and device type for each raw drive
    for i in range(8):
        resp['paths'].append(get_setting("PATH_RAW_" + str(i), ""))
        resp['dev_types'].append(get_setting("ACSI_DEVTYPE_" + str(i), 0))

    return resp


@config.route('/set_ids', methods=['PUT'])
def set_ids():
    """ store paths for ACSI IDs """
    data = request.get_json(force=True)
    paths = data['paths']
    dev_types = data['dev_types']

    # set path and device type for each raw drive
    for i in range(8):
        set_setting("PATH_RAW_" + str(i), paths[i])
        set_setting("ACSI_DEVTYPE_" + str(i), dev_types[i])

    send_to_core_hdd({'module': 'disks', 'action': 'reload_raw'})
    return {'status': 'ok'}


@config.route('/get_dir_content', methods=['POST'])
def get_dir_content():
    """ get content of supplied directory and return it as hash with dirs and files separately """
    data = request.get_json(force=True)
    path = data['path']

    resp = {'path': 'path', 'dirs': [], 'files': []}

    for f in listdir(path):         # walk this dir
        full_path = join(path, f)

        # append to files if it's a file or a block dev
        if (isfile(full_path) or is_blockdev(full_path)) and not f.startswith('.'):
            resp['files'].append(f)

        # append to dirs if it's a dir
        if isdir(full_path) and not f.startswith('.'):
            resp['dirs'].append(f)

    resp['dirs'] = sorted(resp['dirs'], key=str.casefold)
    resp['files'] = sorted(resp['files'], key=str.casefold)

    return resp


@config.route('/get_drives', methods=['GET'])
def get_drives():
    """ get paths for GEM drives """
    resp = {'paths': [], 'drive_types': []}

    conf_drive_letter = get_setting("DRIVELETTER_CONFDRIVE", "O")

    if isinstance(conf_drive_letter, str) and len(conf_drive_letter) > 0:       # if config drive letter is a string that is not empty, extract just first letter
        conf_drive_letter = conf_drive_letter[0]

    app.logger.debug(f'get_drives - conf_drive_letter: {conf_drive_letter}')

    # get path and device type for each raw drive
    for i in range(16):
        drive_letter = chr(65 + i)

        path, drive_type = "", 0

        if drive_letter == conf_drive_letter:   # if this is a config drive letter, it's a config drive
            drive_type = 2
        else:                                   # get path, if path present then drive is GEM drive, otherwise off
            path = get_setting("PATH_GEM_" + drive_letter, "")
            drive_type = 0 if not path else 1

        resp['paths'].append(path)
        resp['drive_types'].append(drive_type)

    return resp


@config.route('/set_drives', methods=['PUT'])
def set_drives():
    """ set paths for GEM drives """
    data = request.get_json(force=True)
    paths = data['paths']
    drive_types = data['drive_types']
    config_drive_letter = "O"

    # set path and device type for each raw drive
    for i in range(16):
        drive_letter = chr(65 + i)

        if i < 2:                       # ignore drives A and B, don't store them
            continue

        path = paths[i] if drive_types[i] == 1 else ""        # path valid only for drive_types 1, otherwise store empty path
        setting_name = "PATH_GEM_" + drive_letter
        set_setting(setting_name, path)

        if drive_types[i] == 2:         # if this is the config drive, store it's letter
            config_drive_letter = chr(65 + i)

    set_setting("DRIVELETTER_CONFDRIVE", config_drive_letter)
    send_to_core_hdd({'module': 'disks', 'action': 'reload_trans'})

    return {'status': 'ok'}
