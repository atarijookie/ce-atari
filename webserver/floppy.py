import os
from flask import Blueprint, make_response, request, current_app as app, abort
from utils import slot_activate, get_image_slots, text_from_file, slot_insert
from shared import FILE_FLOPPY_ACTIVE_SLOT, FLOPPY_UPLOAD_PATH
from werkzeug.utils import secure_filename

floppy = Blueprint('floppy', __name__)


@floppy.route('/slots', methods=['GET'])
def status():
    image_names = get_image_slots()
    active_slot = text_from_file(FILE_FLOPPY_ACTIVE_SLOT)

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

    file_ext = os.path.splitext(filename)[1]        # get file extension
    file_ext = file_ext.lower()

    if file_ext not in ['.st', '.msa', '.zip']:     # file extension seems wrong?
        abort(400, f'file extension {file_ext} not allowed')

    # TODO: if ZIP, check for image inside ZIP

    file_path = os.path.join(FLOPPY_UPLOAD_PATH, filename)
    app.logger.debug(f"upload_image: floppy image saved to: {file_path}")

    f.save(file_path)

    # TODO: if slot_no has some image uploaded, delete it now, so we won't collect a pile of images

    slot_insert(slot_no, file_path)     # tell core to insert this image
    return 'OK', 204


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
    return '', 204
