import os
import re
import threading
import time
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
list_of_items = []

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
    global list_of_items
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

# ----------------------

def get_list_page(item_start, items_count):
    sublist = list_of_items[item_start : (item_start + items_count)]
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
        urwid.connect_signal(button, 'click', on_list_chosen, index)
        body.append(urwid.AttrMap(button, None, focus_map='reversed'))

    return urwid.ListBox(urwid.SimpleFocusListWalker(body))

def on_list_chosen(button, choice):
    global list_index, list_of_lists
    list_index = choice

    body = []
    body.append(urwid.Text(list_of_lists[list_index]['name'], align='center'))

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
        body.append(btn)

        main.original_widget = urwid.Filler(urwid.Pile(body))
        return

    # list loaded, show it
    sublist = get_list_page(0, 25)
    for item in sublist:
        btn = urwid.Button(item['filename'])
        body.append(btn)

#    done = urwid.Button(u'Ok')
#    urwid.connect_signal(done, 'click', exit_program)

    main.original_widget = urwid.Filler(urwid.Pile(body))

def back_to_main_menu(button):
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
