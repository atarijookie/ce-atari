import os
import re
import math
import json
from flask import Blueprint, request, current_app as app, abort
from utils import get_arg_int, slot_insert, get_image_slots, get_storage_path, send_to_taskq, text_from_file

download = Blueprint('download', __name__)


@download.route('/status', methods=['GET'])
def status():
    """ return status - are images being downloaded? do we have storage for images? """
    have_storage = get_storage_path() is not None

    taskq_status = text_from_file(os.getenv('TASKQ_STATUS_PATH'))
    taskq_status = json.loads(taskq_status)
    dn_floppy = taskq_status.get('download_floppy', {})     # get just status for 'download_floppy' actions
    dn_count = len(dn_floppy.keys())

    # go through all the floppy files being now download, get their progress in list, use lowest
    progresses = [item.get('progress', 0) for item in dn_floppy.values()]
    dn_progress = min(progresses) if progresses else 0

    return {'encoding_ready': 1, 'downloading_count': dn_count, 'downloading_progress': dn_progress,
            'do_we_have_storage': have_storage}


@download.route('/list_of_lists', methods=['GET'])
def list_of_lists():
    """ Read a list of lists from disk """

    lol = read_list_of_lists()
    return lol


@download.route('/image_content', methods=['GET'])
def get_image_content():
    image_filenames = request.args.get('image_filenames')

    if not image_filenames:                             # if mandatory parameter missing, fail
        abort(400, 'image_filenames were not provided')

    lc_to_in = {}           # this dict will hold input filename vs the internal lower-case one
    resp = {}
    if image_filenames:                                 # multiple filenames provided? store them
        image_filenames = image_filenames.split(',')    # split thins string by ','

        for im_fi in image_filenames:           # go through the filenames, convert them to lowercase, store to response
            resp[im_fi] = ''                    # to response

            fname_lc = im_fi.strip().lower()    # to lowercase
            lc_to_in[fname_lc] = im_fi          # to our dict

    lol = read_list_of_lists()                  # read list of lists

    for list_ in lol:                                           # go through the list of lists
        list_of_items = load_list_from_csv(list_['filename'])   # load one list

        for item in list_of_items:  # find                      # go through single list
            fname_lc = item['filename'].strip().lower()         # filename to lowercase

            if fname_lc in lc_to_in.keys():                     # if this filename matches one of wanted filenames
                fname_in = lc_to_in[fname_lc]                   # convert lowercase filename to input filename
                resp[fname_in] = item['content']                # store content

    return resp


def read_list_of_lists():
    # read whole file into memory, split to lines
    file = open(os.getenv('LIST_OF_LISTS_LOCAL'), "r")
    data = file.read()
    file.close()
    lines = data.split("\n")
    lol = []

    # split csv lines into list of dictionaries
    index = 0

    for line in lines:
        cols = line.split(",")

        if len(cols) < 2:                               # not enough cols in this row? skip it
            continue

        item = {'index': index, 'name': cols[0], 'url': cols[1]}    # add {index, name, url} to item
        index += 1

        url_filename = os.path.basename(item['url'])    # get filename from url
        ext = os.path.splitext(url_filename)[1]         # get extension from filename

        list_filename = re.sub(r'\W+', '', item['name'])    # from the list name remove all non alphanumeric chars
        list_filename = list_filename.lower()               # all to lowercase
        list_filename = os.path.join(os.getenv('PATH_TO_LISTS'), list_filename + ext)   # path + filename + extension
        item['filename'] = list_filename

        lol.append(item)

    return lol


def load_list_from_csv(csv_filename):
    list_of_items = []

    # read whole file into memory, split to lines
    file = open(csv_filename, "r")
    data = file.read()
    file.close()

    data = data.replace("<br>", "\n")
    lines = data.split("\n")

    # go through the lines, extract individual items
    for line in lines:
        cols = line.split(",", 2)                       # split to 3 items - url, crc, content (which is also coma-separated, but we want it as 1 piece here)

        if len(cols) < 3:                               # not enough cols in this row? skip it
            continue

        item = {'url': cols[0], 'crc': cols[1], 'content': cols[2]}     # add {name, url} to item

        url_filename = os.path.basename(item['url'])    # get filename from url
        item['filename'] = url_filename

        list_of_items.append(item)

    return list_of_items


def floppy_image_exists(storage_path, image_name):
    """ Function returns true if image exists in storage path. """
    if not storage_path:
        return False, None

    image_path = os.path.join(storage_path, image_name)
    return os.path.exists(image_path), image_path


@download.route('/imagelist', methods=['GET'])
def image_list():
    """ Read a list of images from disk, filter it by search phrase, return only items from specified page """

    list_index = get_arg_int('list_index', 0)
    page = get_arg_int('page', 0)
    search_phrase = request.args.get('search')
    items_per_page = get_arg_int('items_per_page', 10)

    list_of_items_filtered = []
    list_filename = read_list_of_lists()[list_index]['filename']
    list_of_items = load_list_from_csv(list_filename)

    storage_path = get_storage_path()

    for item in list_of_items:              # go through all items in list
        content = item['content'].lower()   # content to lower case

        if not search_phrase or search_phrase in content:       # search phrase not provided or it's found in content
            exist, _ = floppy_image_exists(storage_path, item['filename'])
            item['haveIt'] = exist
            list_of_items_filtered.append(item)                 # append item

    total_items = len(list_of_items_filtered)                   # how many items current list has
    total_pages = math.ceil(total_items / items_per_page)       # how many pages current list has

    item_start = page * items_per_page
    sublist = list_of_items_filtered[item_start: (item_start + items_per_page)]     # get just part of the filtered list
    slots = get_image_slots()

    resp = {'totalPages': total_pages, 'currentPage': page, 'imageList': sublist, 'slots': slots}
    return resp


@download.route('/insert', methods=['GET'])
def insert():
    """ insert floppy image to specified slot """
    image = request.args.get('image')

    # check if the image exists
    storage_path = get_storage_path()
    exits, full_path = floppy_image_exists(storage_path, image)

    if not exits:
        abort(400, f"Image {image} is not present in storage path {storage_path}")

    # send the insert command with full path to image
    slot = get_arg_int('slot', 0)
    slot_insert(slot, full_path)
    return {'status': 'ok'}, 204


@download.route('/download', methods=['GET'])
def download_image():
    image_url = request.args.get('image')                       # url where the image should be download from
    image_filename = os.path.basename(image_url)
    storage_path = get_storage_path()
    destination = os.path.join(storage_path, image_filename)    # destination where the file should be saved to

    item = {'action': 'download_floppy', 'url': image_url, 'destination': destination}
    send_to_taskq(item)
    return {'status': 'ok'}
