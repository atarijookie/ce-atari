import os
import re
from time import time
from utils import send_to_taskq
from loguru import logger as app_log


@app_log.catch
def update_floppy_image_lists(msg):
    app_log.debug(f"update_floppy_image_lists was called")

    # make dir for list-of-lists file and other lists
    lol_dir = os.getenv('PATH_TO_LISTS')
    os.makedirs(lol_dir, exist_ok=True)

    # check if should download this file
    lol_local = os.getenv('LIST_OF_LISTS_LOCAL')
    should_download = should_download_list(lol_local)

    if should_download:  # download file if should
        item = {'action': 'download_list',
                'url': os.getenv('LIST_OF_LISTS_URL'),
                'destination': os.getenv('LIST_OF_LISTS_LOCAL'),
                'after_action': 'after_floppy_image_list_downloaded'}

        send_to_taskq(item)
    else:               # no list of lists download needed, can proceed with downloading individual lists files
        update_floppy_image_lists_from_list()


def update_floppy_image_lists_from_list():
    # read the list of lists and download items from it if needed
    try:
        list_of_lists = read_list_of_lists()
        download_lists(list_of_lists)
    except Exception as ex:
        app_log.warning(f'read_list_of_lists or download_lists failed: {str(ex)}')


def read_list_of_lists():
    # read whole file into memory, split to lines
    lol_local = os.getenv('LIST_OF_LISTS_LOCAL')

    if not os.path.exists(lol_local):       # check if the file exists
        app_log.warning(f"file {lol_local} doesn't exist, not reading list of lists")
        return []

    file = open(lol_local, "r")
    data = file.read()
    file.close()
    lines = data.split("\n")

    list_of_lists = []

    # split csv lines into list of dictionaries
    for line in lines:
        cols = line.split(",")

        if len(cols) < 2:  # not enough cols in this row? skip it
            continue

        item = {'name': cols[0], 'url': cols[1]}  # add {name, url} to item

        url_filename = os.path.basename(item['url'])  # get filename from url
        ext = os.path.splitext(url_filename)[1]  # get extension from filename

        list_filename = re.sub(r'\W+', '', item['name'])    # from the list name remove all non alphanumeric chars
        list_filename = list_filename.lower() + ext         # all to lowercase + extension
        list_path = os.path.join(os.getenv('PATH_TO_LISTS'), list_filename)
        item['filename'] = list_path

        list_of_lists.append(item)

    return list_of_lists


def get_lastcheck_filename(list_filename):
    filename_wo_ext = os.path.splitext(list_filename)[0]  # filename without extension
    filename_lastcheck = filename_wo_ext + ".lastcheck"  # filename.lastcheck holds when this file was last downloaded
    return filename_lastcheck


def save_lastcheck_to_file(list_filename):
    # get filename for the .lastcheck file
    filename_lastcheck = get_lastcheck_filename(list_filename)

    now = time()
    now_str = str(int(now))

    # store the current time to .lastcheck file
    with open(filename_lastcheck, 'wt') as output:
        output.write(now_str)


def should_download_list(list_filename):
    if not os.path.exists(list_filename):  # if file for this list doesn't exist, should download
        app_log.debug(f"should download {list_filename}, because it doesn't exist on disk")
        return True

    filename_lastcheck = get_lastcheck_filename(list_filename)

    # try to load file
    try:
        file = open(filename_lastcheck, "rt")
        data = file.read()
        file.close()
    except Exception as ex:  # failed to open last check filename? should download
        # print("exception: {}".format(ex))
        app_log.debug(f"should download {list_filename}, because reading file from disk failed: {str(ex)}")
        return True

    # try to get last check value from file content
    lastcheck = 0
    try:
        lines = data.split("\n")
        line = lines[0]
        lastcheck = int(line)
    except Exception as ex:  # failed to get last check value? should download
        # print("exception: {}".format(ex))
        app_log.debug(f"should download {list_filename}, because reading the content of file went bonkers")
        return True

    # check if last check was at least 24 hours ago
    now = time()
    dwn_hours_ago = int((now - lastcheck) / 3600)

    should = dwn_hours_ago >= 24  # if the list was downloaded more than day ago? should download
    app_log.debug(f"time since last download: {dwn_hours_ago} hours, should download {list_filename}? {should}")
    return should


def download_lists(list_of_lists):
    """ download lists from internet to local storage """
    app_log.debug("about to download lists")

    for item in list_of_lists:          # go through lists, check if should download, do download if needed
        should_download = should_download_list(item['filename'])  # check if should download this file

        if should_download:     # if should download
            app_log.debug(f"should download {item['filename']}")

            item = {'action': 'download_list',
                    'url': item['url'],
                    'destination': item['filename'],
                    'after_action': 'after_floppy_image_list_downloaded'}   # download and call after_action
            send_to_taskq(item)
        else:                   # if should skip
            app_log.debug(f"not downloading {item['filename']}")


def after_floppy_image_list_downloaded(item):
    """ this should get called after floppy list is downloaded """

    list_filename = item.get('destination')
    save_lastcheck_to_file(list_filename)       # save info about last check for this list

    # when main list-of-list was just downloaded, trigger download of individual lists
    lol_local = os.getenv('LIST_OF_LISTS_LOCAL')
    if list_filename == lol_local:
        app_log.info('Triggering individual lists download after the main list download has finished.')
        update_floppy_image_lists_from_list()
