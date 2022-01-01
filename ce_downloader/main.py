import sys
import os
import re
import threading, queue
import time
import math
import urllib3
import codecs
import urwid
from setproctitle import setproctitle
import subprocess

PATH_TO_LISTS = "/ce/lists/"                                    # where the lists are stored locally
BASE_URL = "http://joo.kie.sk/cosmosex/update/"                 # base url where the lists will be stored online
LIST_OF_LISTS_FILE = "list_of_lists.csv"
LIST_OF_LISTS_URL = BASE_URL + LIST_OF_LISTS_FILE               # where the list of lists is on web
LIST_OF_LISTS_LOCAL = PATH_TO_LISTS + LIST_OF_LISTS_FILE        # where the list of lists is locally

setproctitle("ce_downloader")   # set process title
list_of_lists = []      # list of dictionaries: {name, url, filename}
list_index = 0          # index of list in list_of_lists which will be worked on
list_of_items = []      # list containing all the items from file (unfiltered)
list_of_items_filtered = []     # filtered list of items (based on search string)
pile_current_page = None        # urwid pile containing buttons for current page
search_phrase = ""

main_loop = None        # main loop of the urwid library
text_pages = None       # widget holding the text showing current and total pages
page_current = 1        # currently shown page
text_status = None      # widget holding status text
main_list_pile = None   # the main pile containing buttons with images
last_focus_path = None  # holds last focus path to widget which had focus before going to widget subpage
items_per_page = 19     # how many items per page we should show
screen_width = 80       # should be 40 for ST low, 80 for ST mid

should_run = True
queue_download = queue.Queue()      # queue that holds things to download

# ----------------------

def update_list_of_lists():
    # check if should download this file
    should_download = should_download_list(LIST_OF_LISTS_LOCAL)

    if should_download:     # download file if should
        download_list(LIST_OF_LISTS_URL, LIST_OF_LISTS_LOCAL)

    # check again if should download - True here after download would mean that the list is not available locally or is outdated, but failed udpate
    should_download = should_download_list(LIST_OF_LISTS_LOCAL)
    return (not should_download)        # return success / failure

# ----------------------

def read_list_of_lists():
    global list_of_lists

    # read whole file into memory, split to lines
    file = open(LIST_OF_LISTS_LOCAL, "r")
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
        list_filename = PATH_TO_LISTS + list_filename + ext       # path + filename + extension
        item['filename'] = list_filename

        list_of_lists.append(item)

# ----------------------

def get_lastcheck_filename(list_filename):
    filename_wo_ext = os.path.splitext(list_filename)[0]     # filename without extension   
    filename_lastcheck = filename_wo_ext + ".lastcheck"      # filename.lastcheck holds when this file was last downloaded
    return filename_lastcheck

# ----------------------

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

# ----------------------

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

# ----------------------

def download_lists():
    """ download lists from internet to local storage """
    global list_of_lists

    down_count = 0
    fail_count = 0

    for item in list_of_lists:      # go through lists, check if should download, do download if needed
        should_download = should_download_list(item['filename'])        # check if should download this file

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
        update_status("Failed to download {} lists.".format(fail_count))
    else:                   # nothing downloaded, nothing failed?
        update_status("Lists are up to date.")


def alarm_callback(loop=None, data=None):
    """ this gets called on alarm """
    pass


def update_status(new_status):
    """ call this method to update status bar on screen """
    global text_status, main_loop

    if text_status:     # got status widget? show status
        text_status.set_text(new_status)

    if main_loop:       # if got main loop, trigger alarm to redraw widgets
        main_loop.set_alarm_in(1, alarm_callback)


def download_worker():
    """ download any / all selected images to local storage """
    global queue_download, should_run

    while should_run:
        item = None

        try:
            item = queue_download.get(timeout=0.1)  # get one item to download
        except Exception as ex:                 # we're expecting exception on no item to download
            continue

        storage_path = get_storage_path()       # check if got storage and get path

        if not storage_path:                    # no storage? skip item
            queue_download.task_done()
            continue

        local_path = os.path.join(storage_path, item['filename'])   # create local path

        try:
            update_status("Status: downloading {}".format(item['filename']))

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

        except Exception as ex:
            update_status("Status: {}".format(str(ex)))

        queue_download.task_done()

# ----------------------

def load_list_from_csv(csv_filename):
    global list_of_items, list_of_items_filtered
    list_of_items = []
    list_of_items_filtered = []

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

    # first the filtered list is the same as original list
    list_of_items_filtered = list_of_items

# ----------------------

def get_list_for_current_page():
    """ get one page from filtered list """
    global list_of_items_filtered, items_per_page, page_current

    page_current_zbi = (page_current - 1) if page_current > 1 else 0      # create zero-base index, but page_number could be 0
    item_start = page_current_zbi * items_per_page

    sublist = list_of_items_filtered[item_start : (item_start + items_per_page)]
    return sublist

# ----------------------

def create_main_menu():
    global list_of_lists, text_status

    body = []
    body.append(urwid.Text('>>> CosmosEx Downloader <<<', align='center'))
    body.append(urwid.Text('Choose a list from which you want to download or mount items.'))
    body.append(urwid.Divider())

    for index, item in enumerate(list_of_lists):           # go through the list of lists, extract names, put them in the buttons
        button = urwid.Button(item['name'])
        urwid.connect_signal(button, 'click', on_show_selected_list, index)
        body.append(urwid.AttrMap(button, None, focus_map='reversed'))

    body.append(urwid.Divider())

    if not text_status:
        text_status = urwid.Text("Status: idle")        # text showing status

    body.append(text_status)            # add status widget

    return urwid.ListBox(urwid.SimpleFocusListWalker(body))


def get_item_btn_text(item):
    """ for a specified item retrieve full or partial content """
    global search_phrase, screen_width

    item_content = item['content']          # start with whole content

    if search_phrase:                       # got search phrase? show only part of the content that matches
        content_parts = item['content'].split(',')      # split csv to list

        for part in content_parts:          # go through list
            part_lc = part.lower()          # create lowercase part
            
            if search_phrase in part_lc:    # if phrase found in lowercase part, use original part
                item_content = part

    content_len = screen_width - 4 - 14     # how long the content can be, when button sides and filename take some space
    item_content = item_content[:content_len]   # get only part of the content

    btn_text = "{:13} {}".format(item['filename'], item_content)    # format string as filename + content
    return btn_text


def get_current_page_buttons(as_tuple):
    """ create buttons for the current page number as a list
    as_tuple = False - for new pile, just list of buttons
    as_tuple = True  - for existing pile, as a list of tuples of buttons
    """
    from urwid.widget import WEIGHT

    global items_per_page
    sublist = get_list_for_current_page()       # get items for this page

    buttons = []

    focused_index = None

    for index, item in enumerate(sublist):      # from the items create buttons
        btn_text = get_item_btn_text(item)
        btn = urwid.Button(btn_text)            # button with text
        urwid.connect_signal(btn, 'click', on_image_button_clicked, item)   # attach handler on clicked
        btn = urwid.AttrMap(btn, None, focus_map='reversed')        # set button attributes
        btn = (btn, (WEIGHT, 1)) if as_tuple else btn               # tuple or just the button
        buttons.append(btn)

    padding_cnt = items_per_page - len(sublist) # calculate how much padding at the bottom we need

    for i in range(padding_cnt):                # add all padding as needed
        one = (urwid.Divider(), (WEIGHT, 1)) if as_tuple else urwid.Divider()
        buttons.append(one)

    return buttons, focused_index


def update_pile_with_current_buttons():
    """ the pile_current_page might be already shown on the screen, 
        in that case we need to update the .contents attribude with tuples """
    global pile_current_page

    current_page_buttons, focused_index = get_current_page_buttons(as_tuple=(pile_current_page is not None))

    if pile_current_page is None:       # no current pile? create pile from current page buttons
        pile_current_page = urwid.Pile(current_page_buttons)
    else:                               # we got the pile? update pile content with current page buttons
        pile_current_page.contents = current_page_buttons

    if focused_index is not None:       # got focused position? focus there
        pile_current_page.focus_position = focused_index


def on_image_button_clicked(button, item):
    """ when user clicks on image button """
    global last_focus_path, main_list_pile

    if main_list_pile is not None:
        last_focus_path = main_list_pile.get_focus_path()

    storage_path = get_storage_path()           # check if got storage path

    if not storage_path:                        # no storage path?
        show_no_storage()
        return

    path = os.path.join(storage_path, item['filename'])     # check if got this file

    body = []

    body.append(urwid.Text("file: " + item['filename'], align='center'))   # show filename

    body.append(urwid.Text("content: "))
    contents = item['content'].split(',')       # split csv content to list

    for content in contents:                    # show individual content items as rows
        body.append(urwid.Text("   " + content))

    body.append(urwid.Divider())

    if os.path.exists(path):                    # if exists, show insert options
        for i in range(3):
            slot = i + 1
            btn = urwid.Button("Insert to slot {}".format(slot))
            urwid.connect_signal(btn, 'click', insert_image, (item, slot))
            body.append(urwid.AttrMap(btn, None, focus_map='reversed'))
    else:                                       # if doesn't exist, show download option
        btn = urwid.Button("Download")
        urwid.connect_signal(btn, 'click', download_image, item)
        body.append(urwid.AttrMap(btn, None, focus_map='reversed'))

    # add Back button
    btn = urwid.Button("Back")
    urwid.connect_signal(btn, 'click', show_current_page)
    body.append(urwid.AttrMap(btn, None, focus_map='reversed'))

    main.original_widget = urwid.Filler(urwid.Pile(body))


def download_image(button, item):
    """ when image has been selected for download """
    queue_download.put(item)        # enqueue image
    show_current_page(None)         # show current list page


def insert_image(button, item_slot):
    """ when image should be inserted to slot """
    pass


def show_no_storage():
    """ show warning message that no storage is found """
    body = []

    body.append(urwid.Text("Failed to get storage path."))
    body.append(urwid.Divider())
    body.append(urwid.Text("Attach USB drive or shared network drive and try again."))
    body.append(urwid.Divider())

    btn = urwid.Button("Back")
    urwid.connect_signal(btn, 'click', back_to_main_menu)
    body.append(urwid.AttrMap(btn, None, focus_map='reversed'))

    main.original_widget = urwid.Filler(urwid.Pile(body))
    

def on_show_selected_list(button, choice):
    global list_index, list_of_lists, page_current, search_phrase, last_focus_path
    global text_pages
    list_index = choice     # store the chosen list index

    # find out if we got storage path
    storage_path = get_storage_path()

    if not storage_path:    # no storage path?
        show_no_storage()
        return

    page_current = 1        # start from page 1
    search_phrase = ""      # no search string first
    last_focus_path = None

    # try to load the list into memory
    error = None
    try:
        load_list_from_csv(list_of_lists[list_index]['filename'])
    except Exception as ex:
        error = str(ex)

    # if failed to load, show error
    if error:
        body = []
        body.append(urwid.Text("Failed to load list!"))
        body.append(urwid.Text(error))

        btn = urwid.Button("Back")
        urwid.connect_signal(btn, 'click', back_to_main_menu)
        body.append(urwid.AttrMap(btn, None, focus_map='reversed'))

        main.original_widget = urwid.Filler(urwid.Pile(body))
        return

    show_current_page(None)


def show_current_page(button):
    """ show current images page

    This gets called when:
        - first showing list
        - returning back from image submenu
    """
    global text_pages, search_phrase, text_status
    global last_focus_path, main_list_pile

    body = []
    body.append(urwid.Text(list_of_lists[list_index]['name'], align='center'))
    
    # add search edit line
    widget_search = urwid.Edit("Search: ", edit_text=search_phrase)
    urwid.connect_signal(widget_search, 'change', search_changed)
    body.append(urwid.AttrMap(widget_search, None, focus_map='reversed'))

    # the next row will hold pages related stufff
    pages_row = []

    btn_prev = urwid.Button("prev")       # prev page button
    urwid.connect_signal(btn_prev, 'click', btn_prev_clicked)
    pages_row.append(urwid.AttrMap(btn_prev, None, focus_map='reversed'))

    text_pages = urwid.Text("0/0", align='center')          # pages showing text
    pages_row.append(text_pages)

    btn_next = urwid.Button("next")       # next page button
    urwid.connect_signal(btn_next, 'click', btn_next_clicked)
    pages_row.append(urwid.AttrMap(btn_next, None, focus_map='reversed'))

    btn_main = urwid.Button("back to menu")     # back to main menu button
    urwid.connect_signal(btn_main, 'click', back_to_main_menu)
    pages_row.append(urwid.AttrMap(btn_main, None, focus_map='reversed'))

    body.append(urwid.Columns(pages_row))

    global pile_current_page
    update_pile_with_current_buttons()  # first update pile with current buttons
    body.append(pile_current_page)      # then add the pile with current page to body

    if not text_status:
        text_status = urwid.Text("Status: idle")        # text showing status

    body.append(text_status)

    show_page_text()                    # show the new page text

    main_list_pile = urwid.Pile(body)
    main.original_widget = urwid.Filler(main_list_pile)

    if last_focus_path is not None:
        main_list_pile.set_focus_path(last_focus_path)


def search_changed(widget, search_string):
    """ this gets called when search string changes """
    global list_of_items, list_of_items_filtered, page_current, search_phrase, last_focus_path

    list_of_items_filtered = []
    search_phrase = search_string
    search_phrase_lc = search_string.lower()        # search string to lower case before search

    for item in list_of_items:                      # go through all items in list
        content = item['content'].lower()           # content to lower case

        if search_phrase_lc in content:             # if search string in content found
            list_of_items_filtered.append(item)     # append item

    # now set the 1st page and show page text
    page_current = 1
    last_focus_path = None
    show_page_text()                    # show the new page text
    update_pile_with_current_buttons()  # show the new buttons


def get_total_pages():
    global items_per_page, list_of_items_filtered

    total_items = len(list_of_items_filtered)                            # how many items current list has
    total_pages = math.ceil(total_items / items_per_page)       # how many pages current list has
    return total_pages


def show_page_text():
    global text_pages, page_current
    total_pages = get_total_pages()     # how many pages current list has
    text_pages.set_text("{} / {}".format(page_current, total_pages))


def on_page_change(change_direction):
    global page_current

    total_pages = get_total_pages()     # how many pages current list has

    if total_pages == 0:                # no total pages? no current page
        page_current = 1
    else:                               # some total pages?
        if change_direction > 0:                # to next page?
            if page_current < total_pages:      # not on last page yet? increment
                page_current += 1
        else:                                   # to previous page?
            if page_current > 1:                # not on 1st page yet? decrement
                page_current -= 1

    show_page_text()            # show the new page text


def btn_prev_clicked(button):
    """ on prev page clicked """
    global last_focus_path
    last_focus_path = None

    on_page_change(-1)
    update_pile_with_current_buttons()


def btn_next_clicked(button):
    """ on next page clicked """
    global last_focus_path
    last_focus_path = None

    on_page_change(+1)
    update_pile_with_current_buttons()

# ----------------------
last_storage_path = None

def get_storage_path():
    global last_storage_path

    last_storage_exists = last_storage_path is not None and os.path.exists(last_storage_path)     # path still valid? 

    if last_storage_exists:             # while the last storage exists, keep returning it
        return last_storage_path
   
    # last returned path doesn't exist, check current mounts
    result = subprocess.run(['mount'], stdout=subprocess.PIPE)
    result = codecs.decode(result.stdout)
    mounts = result.split("\n")

    storages = []
    storage_shared = None               # holds path to shared drive
    storage_drive = None                # holds path to first USB storage drive

    for mount in mounts:                # go through the mount lines
        if '/mnt/' not in mount:        # if this whole line does not contain /mnt/ , skip it
            continue

        mount_parts = mount.split(' ')  # split line into parts

        for part in mount_parts:        # find the part that contains the '/mnt/' part
            if '/mnt/' not in part:     # if this is NOT the /mnt/ part, skip it
                continue

            part = os.path.join(part, "floppy_images")  # add floppy images dir to path part (might not exist yet)
            storages.append(part)

            if '/mnt/shared' in part:   # is this a shared mount? remember this path
                storage_shared = part
            else:                       # this is a USB drive mount
                if not storage_drive:   # don't have first drive yet? remember this path
                    storage_drive = part

    if not storages:                    # no storage found? fail here
        last_storage_path = None
        return None

    # if we got here, we definitelly have some storages, but we might not have the required subdir
    # so first check if any of the found storages has the subdir already, and if it does, then use it
    for storage in storages:            # go through the storages and check if the floppy_images subdir exists
        if os.path.exists(storage):     # found storage with existing subdir, use it
            last_storage_path = storage
            return storage

    # if we got here, we got some storages, but none of them has floppy_images subdir, so create one and return path
    storage_use = storage_drive if storage_drive else storage_shared    # use USB drive first if available, otherwise use shared drive    

    subprocess.run(['mkdir', '-p', storage_use])    # create the subdir
    last_storage_path = storage_use                 # remember what we've returned
    return storage_use                              # return it

# ----------------------

def back_to_main_menu(button):
    """ when we should return back to main menu """
    main.original_widget = urwid.Padding(create_main_menu(), left=2, right=2)

def exit_program(button):
    raise urwid.ExitMainLoop()


def alarm_start_threads(loop=None, data=None):
    global thr_download_lists, thr_download_images

    thr_download_lists.start()
    thr_download_images.start()

# ----------------------

# start by getting list of lists
while True:
    good = update_list_of_lists()

    if good:
        break
        
    print("Don't have list of lists, will retry in a while...")
    time.sleep(10)
    
# read list of lists into memory
read_list_of_lists()

main = urwid.Padding(create_main_menu(), left=2, right=2)

top = urwid.Overlay(main, urwid.SolidFill(),
    align='center', width=('relative', 100),
    valign='middle', height=('relative', 100),
    min_width=20, min_height=9)

# threads for updating lists
thr_download_lists = threading.Thread(target=download_lists)
thr_download_images = threading.Thread(target=download_worker)

try:
    main_loop = urwid.MainLoop(top, palette=[('reversed', 'standout', '')])
    main_loop.set_alarm_in(0.5, alarm_start_threads)
    main_loop.run()
except KeyboardInterrupt:
    print("Terminated by keyboard...")

should_run = False
