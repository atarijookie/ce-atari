import time
import re
import os
import logging
import urwid
import urllib3
import socket
import threading
from setproctitle import setproctitle
from downloader import DownloaderView
from image_slots import on_show_image_slots
import shared
from urwid_helpers import create_my_button, create_header_footer
import bson

app_log = logging.getLogger('root')

thr_download_lists = None
thr_download_images = None


def update_list_of_lists():
    # check if should download this file
    should_download = should_download_list(shared.LIST_OF_LISTS_LOCAL)

    if should_download:     # download file if should
        download_list(shared.LIST_OF_LISTS_URL, shared.LIST_OF_LISTS_LOCAL)

    # check again if should download - True here after download would mean that the
    # list is not available locally or is outdated, but failed udpate
    should_download = should_download_list(shared.LIST_OF_LISTS_LOCAL)
    return (not should_download)        # return success / failure


def read_list_of_lists():
    # read whole file into memory, split to lines
    file = open(shared.LIST_OF_LISTS_LOCAL, "r")
    data = file.read()
    file.close()
    lines = data.split("\n")

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
        list_filename = shared.PATH_TO_LISTS + list_filename + ext       # path + filename + extension
        item['filename'] = list_filename

        shared.list_of_lists.append(item)


def get_lastcheck_filename(list_filename):
    filename_wo_ext = os.path.splitext(list_filename)[0]     # filename without extension   
    filename_lastcheck = filename_wo_ext + ".lastcheck"      # filename.lastcheck holds when this file was last downloaded
    return filename_lastcheck


def should_download_list(list_filename):
    if not os.path.exists(list_filename):                   # if file for this list doesn't exist, should download
        return True

    filename_lastcheck = get_lastcheck_filename(list_filename)

    # try to load file
    try:
        file = open(filename_lastcheck, "rt")
        data = file.read()
        file.close()
    except Exception as ex:                 # failed to open last check filename? should download
        #print("exception: {}".format(ex))
        return True

    # try to get last check value from file content
    lastcheck = 0
    try:
        lines = data.split("\n")
        line = lines[0]
        lastcheck = int(line)
    except Exception as ex:                 # failed to get last check value? should download
        #print("exception: {}".format(ex))
        return True

    # check if last check was at least 24 hours ago
    now = time.time()
    diff = (now - lastcheck) / 3600

    return (diff >= 24)                     # if the list was downloaded more than day ago? should download


def download_list(list_url, list_filename):
    # open url
    http = urllib3.PoolManager()
    r = http.request('GET', list_url, preload_content=False)

    with open(list_filename, 'wb') as out:
        while True:
            data = r.read(4096)
            
            if not data:
                break

            out.write(data)

    r.release_conn()

    # get filename for the .lastcheck file
    filename_lastcheck = get_lastcheck_filename(list_filename)

    now = time.time()
    now_str = str(int(now))

    # store the current time to .lastcheck file
    with open(filename_lastcheck,'wt') as output:
        output.write(now_str)


def download_lists():
    """ download lists from internet to local storage """
    down_count = 0
    fail_count = 0

    app_log.debug("about to download lists")

    for item in shared.list_of_lists:      # go through lists, check if should download, do download if needed
        should_download = should_download_list(item['filename'])        # check if should download this file

        app_log.debug(f"should download {item['filename']} - {should_download}")

        if should_download:
            try:
                update_status("Downloading {}".format(item['url']))
                download_list(item['url'], item['filename'])        # start download of list
                down_count += 1                                     # no exception? one more list downloaded
            except Exception as ex:
                fail_count += 1
                update_status("Failed downloading {} : {}".format(item['url'], str(ex)))

    if down_count > 0:      # if something was downloaded
        update_status("Downloaded {} lists.".format(down_count))
    elif fail_count > 0:    # nothing downloaded, just failed?
        update_status("Failed to download {} list(s).".format(fail_count))
    else:                   # nothing downloaded, nothing failed?
        update_status("Lists are up to date.")


def update_status_alarm(loop=None, data=None):
    """ this gets called on alarm to update the status from main thread, not from download thread """
    shared.last_status_string = shared.new_status_string      # this new string is the last one used

    if shared.text_status:                      # got this widget? set new text
        shared.text_status.set_text(shared.new_status_string)

    shared.main_loop.draw_screen()              # force redraw


def update_status(new_status):
    """ call this method to update status bar on screen """
    app_log.debug(f"status: {new_status}")
    shared.new_status_string = (new_status[:58] + '..') if len(new_status) > 60 else new_status       # cut to 60 chars

    if shared.new_status_string != shared.last_status_string:     # if status string has changed since last set
        if shared.main_loop:    # if got main loop, trigger alarm to redraw widgets to do the update from main thread
            shared.main_loop.set_alarm_in(0.1, update_status_alarm)


def download_worker():
    """ download any / all selected images to local storage """
    while shared.should_run:
        item = None

        try:
            item = shared.queue_download.get(timeout=0.1)  # get one item to download
        except Exception as ex:                 # we're expecting exception on no item to download
            continue

        storage_path = shared.get_storage_path()       # check if got storage and get path

        if not storage_path:                    # no storage? skip item
            shared.queue_download.task_done()
            continue

        local_path = os.path.join(storage_path, item['filename'])   # create local path

        try:
            status = "Status: downloading {}".format(item['filename'])
            app_log.debug(status)
            update_status(status)

            # open url
            http = urllib3.PoolManager()
            r = http.request('GET', item['url'], preload_content=False)

            with open(local_path, 'wb') as out:     # open local path
                while True:
                    data = r.read(4096)
                    
                    if not data:
                        break

                    out.write(data)                 # write data to file

            r.release_conn()
            update_status("Status: idle")
            app_log.debug("download of {} finished".format(item['filename']))

        except Exception as ex:
            app_log.debug("failed to download {} - ".format(item['filename'], str(ex)))
            update_status("Status: {}".format(str(ex)))

        shared.queue_download.task_done()


def send_to_core_worker():
    """ send messages to core """
    while shared.should_run:
        item = None

        try:
            item = shared.queue_send.get(timeout=0.1)  # get one item to download
        except Exception as ex:     # we're expecting exception on no item to send
            continue

        try:
            app_log.debug(f"sending {item}")
            b_item = bson.dumps(item)                # dict to bson encoded blob

            sckt = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sckt.connect(shared.core_sock_name)
            sckt.send(b_item)
            sckt.close()

        except Exception as ex:
            app_log.debug(f"failed to send {item} - {str(ex)}")

        shared.queue_send.task_done()


def on_show_selected_list(button, choice):
    shared.view_object = DownloaderView()
    shared.view_object.on_show_selected_list(button, choice)


def create_main_menu():
    body = []
    header, footer = create_header_footer('>>> CosmosEx Floppy Tool <<<')

    shared.on_unhandled_keys_handler = None     # no unhandled keys handler on main menu

    for i in range(3):
        body.append(urwid.Divider())

    button = create_my_button('Image slots', on_show_image_slots)
    body.append(button)
    body.append(urwid.Divider())
    body.append(urwid.Divider())

    body.append(urwid.Text('Games to download:'))
    body.append(urwid.Divider())

    for index, item in enumerate(shared.list_of_lists):           # go through the list of lists, extract names, put them in the buttons
        button = create_my_button(item['name'], on_show_selected_list, index)
        body.append(button)

    body.append(urwid.Divider())

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)
    return urwid.Frame(w_body, header=header, footer=footer)


if __name__ == "__main__":
    setproctitle("ce_fdd_py")  # set process title

    for dir in [shared.PATH_TO_LISTS, shared.DATA_DIR, shared.LOG_DIR]:
        os.makedirs(dir, exist_ok=True)

    shared.log_config()

    shared.terminal_cols, terminal_rows = urwid.raw_display.Screen().get_cols_rows()
    shared.items_per_page = terminal_rows - 4

    # start by getting list of lists
    while True:
        good = update_list_of_lists()

        if good:
            break

        print("Don't have list of lists, will retry in a while...")
        time.sleep(10)

    # read list of lists into memory
    read_list_of_lists()

    # threads for updating lists
    thr_download_lists = threading.Thread(target=download_lists)
    thr_download_images = threading.Thread(target=download_worker)
    thr_send_to_core = threading.Thread(target=send_to_core_worker)

    thr_download_lists.start()
    thr_download_images.start()
    thr_send_to_core.start()

    shared.main = urwid.Padding(create_main_menu())

    top = urwid.Overlay(shared.main, urwid.SolidFill(), align='center', width=('relative', 100), valign='middle',
                        height=('relative', 100), min_width=20, min_height=9)

    shared.current_body = top

    try:
        shared.main_loop = urwid.MainLoop(top, palette=[('reversed', 'standout', '')],
                                          unhandled_input=shared.on_unhandled_keys_generic)

        shared.main_loop.run()
    except KeyboardInterrupt:
        print("Terminated by keyboard...")

    shared.should_run = False
