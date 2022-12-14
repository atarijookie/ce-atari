import os
import urwid
import logging
import shared
from urwid_helpers import create_my_button, dialog, back_to_main_menu, create_header_footer
from shared import load_list_from_csv
from file_view import FilesView

app_log = logging.getLogger()

widget_image_name = []
txt_image_name = []                         # fetch current image names here

widget_image_content = []
txt_image_content = []                      # fetch content by image name here

image_name_to_content = {}              # holds dict with image name to content, e.g. {'

screen_shown = False
last_slots_mod_time = 0


def alarm_callback_refresh(loop=None, data=None):
    global screen_shown, last_slots_mod_time

    if not screen_shown:        # this screen is already hidden, don't do the rest
        return

    slots_mod_time = 0
    try:
        slots_mod_time = os.path.getmtime(shared.FILE_SLOTS)    # get modification time
    except Exception as ex:
        app_log.warning(f"slots_mod_time - failed: {str(ex)}")

    if slots_mod_time != last_slots_mod_time:               # modification time changed?
        app_log.debug('alarm_callback_refresh - reloading!')
        last_slots_mod_time = slots_mod_time
        on_update_image_slots(None)

    shared.main_loop.set_alarm_in(1, alarm_callback_refresh)        # check for changes in 1 second


def load_image_contents():
    """ load images content if needed """
    global image_name_to_content

    if image_name_to_content:       # if already got images content, just quit
        return

    for img_list in shared.list_of_lists:       # go through all the image lists
        try:
            list_of_items = load_list_from_csv(img_list['filename'])        # get content from file to list

            for item in list_of_items:          # extract filename-to-content to dictionary
                image_name_to_content[item['filename'].strip().lower()] = item['content'].strip()

        except Exception as ex:
            app_log.warning(f"failed to load list {img_list} - {str(ex)}")


def get_content_for_image_name(image_name):
    load_image_contents()                               # load lists if needed
    image_name = image_name.strip().lower()
    content = image_name_to_content.get(image_name, '')    # get content for filename
    app_log.debug(f"image_name: {image_name} , content: {content}")
    return content


def get_image_slots():
    # first get the image names that are in slot 1, 2, 3
    global txt_image_name, txt_image_content
    txt_image_name = []
    txt_image_content = []

    try:
        with open(shared.FILE_SLOTS, 'rt') as f:
            txt_image_name = f.readlines()

    except Exception as ex:
        app_log.warning(f"Failed to open file {shared.FILE_SLOTS} : {str(ex)}")

    # now get images content for slot 1, 2, 3
    for i in range(3):
        if not i < len(txt_image_name):     # don't have this item at index i? quit
            break

        image_name = txt_image_name[i].strip()
        image_name = os.path.basename(image_name)           # get just filename from the path
        txt_image_name[i] = image_name
        content = get_content_for_image_name(image_name)    # get content (game names) for this image name
        txt_image_content.append(content)


def on_show_image_slots(button):
    header, footer = create_header_footer('Floppy Image Slots')

    global screen_shown

    if not screen_shown:        # if this is create (screen not already shown), start refresh in 1 second
        shared.main_loop.set_alarm_in(1, alarm_callback_refresh)
        screen_shown = True

    # get current images and their content
    get_image_slots()

    body = []
    body.append(urwid.Divider())

    global txt_image_name, widget_image_name, txt_image_content, widget_image_content, last_slots_mod_time

    try:
        last_slots_mod_time = 0
        last_slots_mod_time = os.path.getmtime(shared.FILE_SLOTS)
    except Exception as ex:
        app_log.warning(f"last_slots_mod_time - failed: {str(ex)}")

    for i in range(3):
        app_log.debug(f'index {i}')

        # create buttons and texts for image # i
        btn_insert = create_my_button("Insert", on_insert, i)
        btn_eject = create_my_button("Eject", on_eject, i)

        txt_name = txt_image_name[i] if i < len(txt_image_name) else None
        txt_name = txt_name if txt_name else '(  empty  )'

        app_log.debug(f'txt_name: {txt_name}')

        widget_image_name.append(urwid.Text(txt_name))

        cols = urwid.Columns([
            ('fixed', 6, urwid.AttrMap(urwid.Text(f'Slot {i + 1}'), 'reversed')),
            ('fixed', 12, widget_image_name[i]),
            ('fixed', 10, btn_insert),
            ('fixed', 9, btn_eject)
            ], dividechars=1)

        body.append(cols)

        # create text widget for image # i content
        image_content_text = txt_image_content[i] if i < len(txt_image_content) else ''
        app_log.debug(f'image_content_text: {image_content_text}')

        widget_image_content.append(urwid.Text(image_content_text))

        body.append(widget_image_content[i])
        body.append(urwid.Divider())

    # now just main menu button and we're done
    button_back = create_my_button("Main menu", on_back_to_main_menu)
    buttons = urwid.GridFlow([button_back], 13, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)
    shared.main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def on_update_image_slots(button):
    """ replace old image names and content with new ones """
    global screen_shown
    if not screen_shown:        # if this is create (screen not already shown), start refresh in 1 second
        return

    # get current images and their content
    get_image_slots()

    global txt_image_name, widget_image_name, txt_image_content, widget_image_content

    for i in range(3):
        app_log.debug(f'index {i}')

        txt_name = txt_image_name[i] if i < len(txt_image_name) else None
        txt_name = txt_name if txt_name else '(  empty  )'

        app_log.debug(f'txt_name: {txt_name}')
        widget_image_name[i].set_text(txt_name)

        # create text widget for image # i content
        image_content_text = txt_image_content[i] if i < len(txt_image_content) else ''
        app_log.debug(f'image_content_text: {image_content_text}')

        widget_image_content[i].set_text(image_content_text)


def on_back_to_main_menu(button):
    global screen_shown
    screen_shown = False
    back_to_main_menu(button)


def on_insert(button, index):
    shared.view_object = FilesView()
    shared.view_object.set_fdd_slot(index)      # tell files view to which slot it should insert image
    shared.view_object.on_show_selected_list(button, '/mnt')


def on_eject(button, index):
    """ eject image from slot """
    shared.slot_eject(index)
