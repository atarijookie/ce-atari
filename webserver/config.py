import os
import re
import math
import json
from os import listdir
from os.path import isfile, isdir, join

from flask import Blueprint, request, current_app as app, abort
from utils import get_setting, set_setting

config = Blueprint('config', __name__)


@config.route('/get_ids', methods=['GET'])
def get_ids():
    resp = {'paths': [], 'dev_types': []}

    # get path and device type for each raw drive
    for i in range(8):
        resp['paths'].append(get_setting("RAW_DRIVE_" + str(i), ""))
        resp['dev_types'].append(get_setting("ACSI_DEVTYPE_" + str(i), 0))

    return resp

@config.route('/set_ids', methods=['PUT'])
def set_ids():
    data = request.get_json(force=True)
    paths = data['paths']
    dev_types = data['dev_types']

    # set path and device type for each raw drive
    for i in range(8):
        set_setting("RAW_DRIVE_" + str(i), paths[i])
        set_setting("ACSI_DEVTYPE_" + str(i), dev_types[i])

    return {'status': 'ok'}, 204

@config.route('/get_dir_content', methods=['POST'])
def get_dir_content():
    data = request.get_json(force=True)
    path = data['path']

    resp = {'path': 'path', 'dirs': [], 'files': []}

    for f in listdir(path):
        if isfile(join(path, f)) and not f.startswith('.'):
            resp['files'].append(f)

        if isdir(join(path, f)) and not f.startswith('.'):
            resp['dirs'].append(f)

    resp['dirs'] = sorted(resp['dirs'], key=str.casefold)
    resp['files'] = sorted(resp['files'], key=str.casefold)

    return resp
