import os
from flask import Blueprint, make_response, request, current_app as app, abort
from utils import slot_insert, file_seems_to_be_image, \
    unlink_without_fail, symlink_if_needed, get_setting, set_setting, send_to_core_fdd, text_from_file
from werkzeug.utils import secure_filename

floppy = Blueprint('floppy', __name__)


@floppy.route('/<int:slot_no>', methods=['POST'])
def upload_image(slot_no):
    # 0-2 - regular slots upload

    if slot_no not in [0, 1, 2]:                    # slot number is invalid?
        abort(400, f'invalid slot number: {slot_no}')

    if 'file' not in request.files:                 # if file not uploaded
        abort(400, 'file not in request.files')

    f = request.files['file']
    filename = secure_filename(f.filename)          # create secure filename

    floppy_upload_path = os.getenv('FLOPPY_UPLOAD_PATH')
    file_path = os.path.join(floppy_upload_path, filename)
    f.save(file_path)
    app.logger.debug(f"upload_image: floppy image saved to: {file_path}")

    # check if the file extension is supported
    success, message = file_seems_to_be_image(file_path, True)

    if not success:     # not a valid image? fail here
        app.logger.warning(f"upload_image: not a valid image, deleting it and failing: {message}")
        os.unlink(file_path)
        abort(400, message)

    # if slot_no has some image uploaded, delete it now, so we won't collect a pile of images
    symlink_slot_path = os.path.join(floppy_upload_path, f"image_in_slot_{slot_no}")

    # get link to old file if possible
    link_source_path = None
    if os.path.exists(symlink_slot_path) and os.path.islink(symlink_slot_path):
        link_source_path = os.readlink(symlink_slot_path)

    # if old file is not the same as new file, delete file
    if link_source_path and link_source_path != file_path:
        app.logger.debug(f"upload_image: deleting previous image: {link_source_path}")
        unlink_without_fail(link_source_path)

    unlink_without_fail(symlink_slot_path)              # delete old link
    symlink_if_needed(file_path, symlink_slot_path)     # create new symlink

    slot_insert(slot_no, file_path)     # tell core to insert this image
    return {'status': 'ok'}


@floppy.route('/<int:slot_no>', methods=['PUT'])
def set_floppy_slot(slot_no):
    """ set new image path for specified slot_no """

    data = request.get_json(force=True)
    path = data['path']

    # check if the file extension is supported
    success, message = file_seems_to_be_image(path, True)

    if not success:     # not a valid image? fail here
        app.logger.warning(f"set_floppy_slot: not a valid image, failing: {message}")
        abort(400, message)
    
    set_setting("FLOPPY_IMAGE_" + str(slot_no), path)
    send_to_core_fdd({'module': 'floppy', 'action': 'insert', 'slot': slot_no, 'image': path})

    return {'status': 'ok'}


@floppy.route('/get_slots', methods=['GET'])
def get_slots():
    """ get paths for floppy images in slots """

    resp = {'max_clients': 8, 'paths': []}
    max_clients = 8

    # get max clients from env file
    try:
        max_clients = os.getenv('FDD_MAX_CLIENTS')

        if not max_clients:
            max_clients = 8

        max_clients = int(max_clients)
        resp['max_clients'] = max_clients
    except ValueError:                  # if not int, fail
        abort(400, 'conversion to int failed')

    # get path and device type for each raw drive
    for i in range(max_clients):
        path = get_setting("FLOPPY_IMAGE_" + str(i), "")
        resp['paths'].append(path)

    return resp


@floppy.route('/get_clients', methods=['GET'])
def get_clients():
    """ get info about connected clients """

    path = os.path.join(os.getenv('FILE_FLOPPY_SLOTS'), "/tmp/ce/data/floppy_slots.json")
    resp = text_from_file(path)
    return resp
