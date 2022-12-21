import os
import re
import math
from flask import Blueprint, make_response, request, current_app as app
from utils import get_arg_int, slot_insert, get_image_slots
from shared import LIST_OF_LISTS_LOCAL, PATH_TO_LISTS

download = Blueprint('download', __name__)


@download.route('/status', methods=['GET'])
def status():
    return ''


def read_list_of_lists():
    # read whole file into memory, split to lines
    file = open(LIST_OF_LISTS_LOCAL, "r")
    data = file.read()
    file.close()
    lines = data.split("\n")
    list_of_lists = []

    # split csv lines into list of dictionaries
    for line in lines:
        cols = line.split(",")

        if len(cols) < 2:                               # not enough cols in this row? skip it
            continue

        item = {'name': cols[0], 'url': cols[1]}        # add {name, url} to item

        url_filename = os.path.basename(item['url'])    # get filename from url
        ext = os.path.splitext(url_filename)[1]         # get extension from filename

        list_filename = re.sub(r'\W+', '', item['name'])    # from the list name remove all non alphanumeric chars
        list_filename = list_filename.lower()               # all to lowercase
        list_filename = os.path.join(PATH_TO_LISTS, list_filename + ext)       # path + filename + extension
        item['filename'] = list_filename

        list_of_lists.append(item)

    return list_of_lists


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


@download.route('/imagelist', methods=['GET'])
def image_list():
    list_index = get_arg_int('list_index', 0)
    page = get_arg_int('page', 0)
    search_phrase = request.args.get('search')
    items_per_page = get_arg_int('items_per_page', 10)

    list_of_items_filtered = []
    list_filename = read_list_of_lists()[list_index]['filename']
    list_of_items = load_list_from_csv(list_filename)

    for item in list_of_items:              # go through all items in list
        content = item['content'].lower()   # content to lower case

        if not search_phrase or search_phrase in content:       # search phrase not provided or it's found in content
            list_of_items_filtered.append(item)                 # append item

    total_items = len(list_of_items_filtered)                   # how many items current list has
    total_pages = math.ceil(total_items / items_per_page)       # how many pages current list has

    item_start = page * items_per_page
    sublist = list_of_items_filtered[item_start: (item_start + items_per_page)]     # get just part of the filtered list
    slots = get_image_slots()

    return {'totalPages': total_pages, 'currentPage': page, 'imageList': sublist, 'slots': slots}


@download.route('/insert', methods=['GET'])
def insert():
    image = request.args.get('image')
    slot = get_arg_int('slot', 0)
    slot_insert(slot, image)
    return '', 204


@download.route('/download', methods=['GET'])
def download_image():
    image = request.args.get('image')
    return ''
