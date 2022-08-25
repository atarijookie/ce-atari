import os
import queue
import math
import codecs
import urwid
import subprocess
import logging
import shared
from urwid_helpers import create_my_button, dialog, back_to_main_menu, create_header_footer

app_log = logging.getLogger()

queue_download = queue.Queue()      # queue that holds things to download


def show_storage_read_only():
    """ show warning message that cannot write storage """
    dialog(shared.main_loop, shared.current_body,
           "Cannot write to storage path.\nYou won't be able to download images, "
           "and even might have issues loading your images.")


def show_no_storage():
    """ show warning message that no storage is found """
    dialog(shared.main_loop, shared.current_body,
           f"Failed to get storage path. Attach USB drive or shared network drive and try again.")


def can_write_to_storage(path):
    """ check if can write to storage path """
    test_file = os.path.join(path, "test_file.txt")

    try:        # first remove the test file if it exists
        os.remove(test_file)
    except:     # don't fail if failed to remove file - might not exist at this time
        pass

    good = False

    try:        # create file and write to file
        with open(test_file, 'wt') as out:
            out.write("whatever")

        good = True
    except:     # this error is expected
        pass

    try:        # remove the test file if it exists
        os.remove(test_file)
    except:     # don't fail if failed to remove file - might not exist at this time
        pass

    return good


def on_show_selected_list(button, choice):
    shared.list_index = choice     # store the chosen list index

    # find out if we got storage path
    storage_path = get_storage_path()

    if not storage_path:    # no storage path?
        show_no_storage()
        return

    if not can_write_to_storage(storage_path):
        show_storage_read_only()
        return

    shared.page_current = 1        # start from page 1
    shared.search_phrase = ""      # no search string first
    shared.last_focus_path = None

    # try to load the list into memory
    error = None
    try:
        load_list_from_csv(shared.list_of_lists[shared.list_index]['filename'])
    except Exception as ex:
        error = str(ex)

    # if failed to load, show error
    if error:
        dialog(shared.main_loop, shared.current_body, f"Failed to load list!\n{error}")
        return

    show_current_page(None)


def show_current_page(button):
    """ show current images page

    This gets called when:
        - first showing list
        - returning back from image submenu
    """
    body = []
    body.append(urwid.Text(shared.list_of_lists[shared.list_index]['name'], align='center'))

    # add search edit line
    widget_search = urwid.Edit("Search: ", edit_text=shared.search_phrase)
    urwid.connect_signal(widget_search, 'change', search_changed)
    body.append(urwid.AttrMap(widget_search, None, focus_map='reversed'))

    # the next row will hold pages related stufff
    pages_row = []

    btn_prev = create_my_button("prev", btn_prev_clicked)
    pages_row.append(btn_prev)

    shared.text_pages = urwid.Text("0/0", align='center')  # pages showing text
    pages_row.append(shared.text_pages)

    btn_next = create_my_button("next", btn_next_clicked)
    pages_row.append(btn_next)

    btn_main = create_my_button("back to menu", back_to_main_menu)
    pages_row.append(btn_main)

    body.append(urwid.Columns(pages_row))

    update_pile_with_current_buttons()  # first update pile with current buttons
    body.append(shared.pile_current_page)  # then add the pile with current page to body

    if not shared.text_status:
        shared.text_status = urwid.Text("Status: idle")  # text showing status

    body.append(shared.text_status)

    show_page_text()  # show the new page text

    shared.main_list_pile = urwid.Pile(body)
    shared.main.original_widget = urwid.Filler(shared.main_list_pile)

    if shared.last_focus_path is not None:
        shared.main_list_pile.set_focus_path(shared.last_focus_path)

def search_changed(widget, search_string):
    """ this gets called when search string changes """
    shared.list_of_items_filtered = []
    shared.search_phrase = search_string
    shared.search_phrase_lc = search_string.lower()        # search string to lower case before search

    for item in shared.list_of_items:                      # go through all items in list
        content = item['content'].lower()           # content to lower case

        if shared.search_phrase_lc in content:             # if search string in content found
            shared.list_of_items_filtered.append(item)     # append item

    # now set the 1st page and show page text
    shared.page_current = 1
    shared.last_focus_path = None
    show_page_text()                    # show the new page text
    update_pile_with_current_buttons()  # show the new buttons


def get_total_pages():
    total_items = len(shared.list_of_items_filtered)                   # how many items current list has
    total_pages = math.ceil(total_items / shared.items_per_page)       # how many pages current list has
    return total_pages


def show_page_text():
    total_pages = get_total_pages()     # how many pages current list has
    shared.text_pages.set_text("{} / {}".format(shared.page_current, total_pages))


def on_page_change(change_direction):
    total_pages = get_total_pages()     # how many pages current list has

    if total_pages == 0:                # no total pages? no current page
        shared.page_current = 1
    else:                               # some total pages?
        if change_direction > 0:                # to next page?
            if shared.page_current < total_pages:      # not on last page yet? increment
                shared.page_current += 1
        else:                                   # to previous page?
            if shared.page_current > 1:                # not on 1st page yet? decrement
                shared.page_current -= 1

    show_page_text()            # show the new page text


def btn_prev_clicked(button):
    """ on prev page clicked """
    shared.last_focus_path = None

    on_page_change(-1)
    update_pile_with_current_buttons()


def btn_next_clicked(button):
    """ on next page clicked """
    shared.last_focus_path = None

    on_page_change(+1)
    update_pile_with_current_buttons()


def download_image(button, item):
    """ when image has been selected for download """
    app_log.debug("download image {}".format(item['filename']))
    shared.queue_download.put(item)        # enqueue image
    show_current_page(None)         # show current list page


def insert_image(button, item_slot):
    """ when image should be inserted to slot """
    item, slot = item_slot
    app_log.debug("insert image {} to slot {}".format(item['filename'], slot))


def get_storage_path():
    last_storage_exists = shared.last_storage_path is not None and os.path.exists(
        shared.last_storage_path)  # path still valid?

    if last_storage_exists:  # while the last storage exists, keep returning it
        return shared.last_storage_path

    # last returned path doesn't exist, check current mounts
    result = subprocess.run(['mount'], stdout=subprocess.PIPE)
    result = codecs.decode(result.stdout)
    mounts = result.split("\n")

    storages = []
    storage_shared = None  # holds path to shared drive
    storage_drive = None  # holds path to first USB storage drive

    for mount in mounts:  # go through the mount lines
        if '/mnt/' not in mount:  # if this whole line does not contain /mnt/ , skip it
            continue

        mount_parts = mount.split(' ')  # split line into parts

        for part in mount_parts:  # find the part that contains the '/mnt/' part
            if '/mnt/' not in part:  # if this is NOT the /mnt/ part, skip it
                continue

            part = os.path.join(part, "floppy_images")  # add floppy images dir to path part (might not exist yet)
            storages.append(part)

            if '/mnt/shared' in part:  # is this a shared mount? remember this path
                storage_shared = part
            else:  # this is a USB drive mount
                if not storage_drive:  # don't have first drive yet? remember this path
                    storage_drive = part

    if not storages:  # no storage found? fail here
        shared.last_storage_path = None
        return None

    # if we got here, we definitelly have some storages, but we might not have the required subdir
    # so first check if any of the found storages has the subdir already, and if it does, then use it
    for storage in storages:  # go through the storages and check if the floppy_images subdir exists
        if os.path.exists(storage):  # found storage with existing subdir, use it
            shared.last_storage_path = storage
            return storage

    # if we got here, we got some storages, but none of them has floppy_images subdir, so create one and return path
    storage_use = storage_drive if storage_drive else storage_shared  # use USB drive first if available, otherwise use shared drive

    subprocess.run(['mkdir', '-p', storage_use])  # create the subdir
    shared.last_storage_path = storage_use  # remember what we've returned
    return storage_use  # return it


def load_list_from_csv(csv_filename):
    shared.list_of_items = []
    shared.list_of_items_filtered = []

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

        shared.list_of_items.append(item)

    # first the filtered list is the same as original list
    shared.list_of_items_filtered = shared.list_of_items

def update_pile_with_current_buttons():
    """ the shared.pile_current_page might be already shown on the screen,
        in that case we need to update the .contents attribude with tuples """
    current_page_buttons, focused_index = get_current_page_buttons(as_tuple=(shared.pile_current_page is not None))

    if shared.pile_current_page is None:       # no current pile? create pile from current page buttons
        shared.pile_current_page = urwid.Pile(current_page_buttons)
    else:                               # we got the pile? update pile content with current page buttons
        shared.pile_current_page.contents = current_page_buttons

    if focused_index is not None:       # got focused position? focus there
        shared.pile_current_page.focus_position = focused_index


def on_image_button_clicked(button, item):
    """ when user clicks on image button """
    if shared.main_list_pile is not None:
        shared.last_focus_path = shared.main_list_pile.get_focus_path()

    storage_path = get_storage_path()           # check if got storage path

    if not storage_path:                        # no storage path?
        show_no_storage()
        return

    if not can_write_to_storage(storage_path):
        show_storage_read_only()
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
            btn = create_my_button("Insert to slot {}".format(slot), insert_image, (item, slot))
            body.append(btn)
    else:                                       # if doesn't exist, show download option
        btn = create_my_button("Download", download_image, item)
        body.append(btn)

    # add Back button
    btn = create_my_button("Back", show_current_page)
    body.append(btn)

    shared.main.original_widget = urwid.Filler(urwid.Pile(body))


def get_current_page_buttons(as_tuple):
    """ create buttons for the current page number as a list
    as_tuple = False - for new pile, just list of buttons
    as_tuple = True  - for existing pile, as a list of tuples of buttons
    """
    from urwid.widget import WEIGHT

    sublist = get_list_for_current_page()       # get items for this page

    buttons = []

    focused_index = None

    for index, item in enumerate(sublist):      # from the items create buttons
        btn_text = get_item_btn_text(item)
        btn = create_my_button(btn_text, on_image_button_clicked, item)   # attach handler on clicked
        btn = (btn, (WEIGHT, 1)) if as_tuple else btn               # tuple or just the button
        buttons.append(btn)

    padding_cnt = shared.items_per_page - len(sublist) # calculate how much padding at the bottom we need

    for i in range(padding_cnt):                # add all padding as needed
        one = (urwid.Divider(), (WEIGHT, 1)) if as_tuple else urwid.Divider()
        buttons.append(one)

    return buttons, focused_index


def get_list_for_current_page():
    """ get one page from filtered list """
    shared.page_current_zbi = (shared.page_current - 1) if shared.page_current > 1 else 0      # create zero-base index, but page_number could be 0
    item_start = shared.page_current_zbi * shared.items_per_page

    sublist = shared.list_of_items_filtered[item_start : (item_start + shared.items_per_page)]
    return sublist


def get_item_btn_text(item):
    """ for a specified item retrieve full or partial content """
    item_content = item['content']  # start with whole content

    if shared.search_phrase:  # got search phrase? show only part of the content that matches
        content_parts = item['content'].split(',')  # split csv to list

        for part in content_parts:  # go through list
            part_lc = part.lower()  # create lowercase part

            if shared.search_phrase in part_lc:  # if phrase found in lowercase part, use original part
                item_content = part

    content_len = shared.terminal_cols - 25  # how long the content can be, when button sides and filename take some space
    item_content = item_content[:content_len]  # get only part of the content

    btn_text = "{:13} {}".format(item['filename'], item_content)  # format string as filename + content
    return btn_text

