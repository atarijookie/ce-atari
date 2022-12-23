import os
from flask import Blueprint, make_response, request, current_app as app, abort
from utils import slot_activate, get_image_slots, text_from_file, slot_insert, file_seems_to_be_image, \
    unlink_without_fail, symlink_if_needed
from werkzeug.utils import secure_filename

floppy = Blueprint('floppy', __name__)


@floppy.route('/slots', methods=['GET'])
def status():
    image_names = get_image_slots()
    active_slot = text_from_file(os.getenv('FILE_FLOPPY_ACTIVE_SLOT'))

    try:        # try to convert to int if possible
        active_slot = int(active_slot)
    except Exception as ex:
        app.logger.warning(f"failed to convert {active_slot} to int: {str(ex)}")
        active_slot = None

    return {'slots': image_names, 'active': active_slot}


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
def activate_slot(slot_no):
    """
    :param slot_no: 0-2 - regular slots
                    100 - config image
                    101 - test image
    """

    if slot_no not in [0, 1, 2, 100, 101]:  # invalid slot number? This means deactivate any selected slot.
        slot_no = -1

    slot_activate(slot_no)
    return {'status': 'ok'}
