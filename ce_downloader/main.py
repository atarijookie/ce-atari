import os
import re
import threading
import time
import math
import urllib3
import urwid
from setproctitle import setproctitle

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

text_pages = None       # widget holding the text showing current and total pages
page_current = 1        # currently shown page
items_per_page = 20     # how many items per page we should show
screen_width = 80       # should be 40 for ST low, 80 for ST mid

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

def download_lists(name):
    global list_of_lists

    for item in list_of_lists:      # go through lists, check if should download, do download if needed
        should_download = should_download_list(item['filename'])        # check if should download this file

        if should_download:
            try:
                download_list(item['url'], item['filename'])
            except Exception as ex:
                print("Failed downloading {} : {}".format(item['url'], str(ex)))

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
    global list_of_lists

    body = []
    body.append(urwid.Text('>>> CosmosEx Downloader <<<', align='center'))
    body.append(urwid.Text('Choose a list from which you want to download or mount items.'))
    body.append(urwid.Divider())

    for index, item in enumerate(list_of_lists):           # go through the list of lists, extract names, put them in the buttons
        button = urwid.Button(item['name'])
        urwid.connect_signal(button, 'click', on_show_selected_list, index)
        body.append(urwid.AttrMap(button, None, focus_map='reversed'))

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

    for item in sublist:                        # from the items create buttons
        btn_text = get_item_btn_text(item)
        btn = urwid.AttrMap(urwid.Button(btn_text), None, focus_map='reversed')
        btn = (btn, (WEIGHT, 1)) if as_tuple else btn
        buttons.append(btn)

    padding_cnt = items_per_page - len(sublist) # calculate how much padding at the bottom we need

    for i in range(padding_cnt):                # add all padding as needed
        one = (urwid.Divider(), (WEIGHT, 1)) if as_tuple else urwid.Divider()
        buttons.append(one)

    return buttons


def update_pile_with_current_buttons():
    """ the pile_current_page might be already shown on the screen, 
        in that case we need to update the .contents attribude with tuples """
    global pile_current_page

    current_page_buttons = get_current_page_buttons(as_tuple=(pile_current_page is not None))

    if pile_current_page is None:       # no current pile? create pile from current page buttons
        pile_current_page = urwid.Pile(current_page_buttons)
    else:                               # we got the pile? update pile content with current page buttons
        pile_current_page.contents = current_page_buttons


def on_show_selected_list(button, choice):
    global list_index, list_of_lists
    global text_pages
    list_index = choice     # store the chosen list index

    page_current = 1        # start from page 1

    body = []
    body.append(urwid.Text(list_of_lists[list_index]['name'], align='center'))
    
    # add search edit line
    widget_search = urwid.Edit("Search: ")
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

    # try to load the list into memory
    error = None
    try:
        load_list_from_csv(list_of_lists[list_index]['filename'])
    except Exception as ex:
        error = str(ex)

    # if failed to load, show error
    if error is not None:
        body.append(urwid.Text("Failed to load list!"))
        body.append(urwid.Text(error))

        btn = urwid.Button("Back")
        urwid.connect_signal(btn, 'click', back_to_main_menu)
        body.append(urwid.AttrMap(btn, None, focus_map='reversed'))

        main.original_widget = urwid.Filler(urwid.Pile(body))
        return

    global pile_current_page, search_phrase
    search_phrase = ""                  # no search phrase yet
    update_pile_with_current_buttons()  # first update pile with current buttons
    body.append(pile_current_page)      # then add the pile with current page to body

    show_page_text()                    # show the new page text

    main.original_widget = urwid.Filler(urwid.Pile(body))


def search_changed(widget, search_string):
    """ this gets called when search string changes """
    global list_of_items, list_of_items_filtered, page_current, search_phrase

    list_of_items_filtered = []
    search_phrase = search_string.lower()           # search string to lower case before search

    for item in list_of_items:                      # go through all items in list
        content = item['content'].lower()           # content to lower case

        if search_phrase in content:                # if search string in content found
            list_of_items_filtered.append(item)     # append item

    # now set the 1st page and show page text
    page_current = 1
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
    on_page_change(-1)
    update_pile_with_current_buttons()


def btn_next_clicked(button):
    """ on next page clicked """
    on_page_change(+1)
    update_pile_with_current_buttons()


def back_to_main_menu(button):
    """ when we should return back to main menu """
    main.original_widget = urwid.Padding(create_main_menu(), left=2, right=2)

def exit_program(button):
    raise urwid.ExitMainLoop()

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

# update other lists in threads
thr_download = threading.Thread(target=download_lists, args=(1,))
thr_download.start()

main = urwid.Padding(create_main_menu(), left=2, right=2)

top = urwid.Overlay(main, urwid.SolidFill(),
    align='center', width=('relative', 100),
    valign='middle', height=('relative', 100),
    min_width=20, min_height=9)

try:
    urwid.MainLoop(top, palette=[('reversed', 'standout', '')]).run()
except KeyboardInterrupt:
    print("Terminated by keyboard...")
