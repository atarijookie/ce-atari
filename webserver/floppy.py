from flask import Blueprint, make_response, request, current_app as app, abort
from utils import slot_activate, get_image_slots, text_from_file
from shared import FILE_FLOPPY_ACTIVE_SLOT


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
    return ''


@floppy.route('/<int:slot_no>', methods=['PUT'])
def activate_slot(slot_no):
    """
    :param slot_no: 0-2 - regular slots
                    100 - config image
                    101 - test image
    """

    if slot_no not in [0, 1, 2, 100, 101]:  # invalid slot number?
        abort(400)

    slot_activate(slot_no)
    return '', 204
