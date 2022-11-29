import os
import urwid
import logging
from time import sleep
from urwid_helpers import create_my_button, create_header_footer, create_edit, dialog, dialog_yes_no
from utils import settings_load, settings_save, on_cancel, back_to_main_menu, setting_get_str, on_editline_changed, \
    text_from_file, delete_file, setting_load_one
import shared

app_log = logging.getLogger()
edit_imgpath = None

DATA_DIR = '/var/run/ce/'
FILE_HDDIMAGE_RESOLVED = os.path.join(DATA_DIR, 'HDDIMAGE_RESOLVED')    # where the resolved HDDIMAGE will end up


def hdd_image_create(button):
    settings_load()

    header, footer = create_header_footer('Disk image settings')

    body = []
    body.append(urwid.Divider())

    body.append(urwid.Text('HDD image path on RPi', align='left'))

    global edit_imgpath
    cols, edit_imgpath = create_edit('HDDIMAGE', 40, on_editline_changed)     # hdd image path here
    body.append(cols)

    body.append(urwid.Divider())

    # add save + cancel button
    button_save = create_my_button(" Save", hdd_img_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    button_clear = create_my_button("Clear", hdd_img_clear)
    buttons = urwid.GridFlow([button_save, button_cancel, button_clear], 10, 1, 1, 'center')
    body.append(buttons)

    body.append(urwid.Divider())

    body.append(urwid.Text('Enter here full path to .IMG file. Path ', align='center'))
    body.append(urwid.Text('beginning with shared or usb will be    ', align='center'))
    body.append(urwid.Text('autocompleted. * wildcards are supported', align='center'))
    body.append(urwid.Text('HDD image will be mounted as RAW disk.  ', align='center'))
    body.append(urwid.Text('Ensure you have at least one ACSI ID    ', align='center'))
    body.append(urwid.Text('configured as RAW.                      ', align='center'))
    body.append(urwid.Divider())
    body.append(urwid.Text('Mounting is even easier from Atari:     ', align='center'))
    body.append(urwid.Text('Double-click on .IMG on translated drive', align='center'))
    body.append(urwid.Text('to mount image using CE_HDIMG.TTP tool. ', align='center'))

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)
    shared.main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def hdd_img_clear(button):
    global edit_imgpath
    edit_imgpath.set_edit_text('')              # widget text to empty string

    shared.settings_changed['HDDIMAGE'] = ''    # changed settings to empty string


def hdd_img_save(button):
    app_log.debug(f"shared_drive_save: {shared.settings_changed}")

    path_image_current = setting_load_one('HDDIMAGE')
    path_image_new = setting_get_str('HDDIMAGE')

    # if clear path is specified (user wants to remove hdd image) OR no path change, just save it and return to menu
    if not path_image_new or (path_image_new == path_image_current):
        settings_save()
        back_to_main_menu(None)
        return

    delete_file(FILE_HDDIMAGE_RESOLVED)     # delete the resolved image file, so we won't respond to existing file

    settings_save()                 # save the value to file, let mounter try to resolve it
    did_resolve = False
    exists = False

    for i in range(20):             # for some time try to see if mounter was able to resolve the HDD image filename
        sleep(0.1)
        resolved_filename = text_from_file(FILE_HDDIMAGE_RESOLVED)

        if not resolved_filename:       # still not resolved, try again
            continue

        exists = os.path.exists(resolved_filename)
        did_resolve = True
        break

    if not did_resolve:
        dialog(shared.main_loop, shared.current_body, f"Image filename not resolved.\nIs mounter running?")
        return

    # no valid path was found? show message and don't save
    if not exists:
        dialog(shared.main_loop, shared.current_body, f"The path is invalid:\n{path_image_new}")
        return

    # path is pointing to shared drive? warn user
    if path_image_new.startswith("shared/"):
        dialog_yes_no(shared.main_loop, shared.current_body,
                      "It is not safe to mount HDD image from network.\nAre you sure?", call_on_answer=on_user_answer)
        return

    # if the input path was resolved to something else, show it
    dialog_yes_no(shared.main_loop, shared.current_body,
                  f"Your path was resolved to:\n{resolved_filename}\nIs this ok?", call_on_answer=on_user_answer)


def on_user_answer(yes_pressed):
    # when user pressed YES, we want to save path and return to main menu
    if yes_pressed:
        save_and_return()


def save_and_return():
    # save path and return to main menu
    settings_save()
    back_to_main_menu(None)
