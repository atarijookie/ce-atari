import os
import urwid
import logging
from urwid_helpers import create_my_button, create_header_footer, create_edit, dialog
from utils import settings_load, settings_save, on_cancel, back_to_main_menu, setting_get_str, on_editline_changed
import shared

app_log = logging.getLogger()
edit_imgpath = None


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

    path = setting_get_str('HDDIMAGE')

    if path.startswith('usb'):          # starts with usb drive path?
        app_log.debug(f"shared_drive_save: starts with usb")

        sub_path = path[3:]

        if sub_path.startswith('/'):    # if has / at start, remove it
            sub_path = sub_path[1:]

        # go through translated drives
        # get mount path for usb drive

        # create full path
        # path = os.path.join(rootpath, subpath)

        # if path exists, use it

        pass

    elif path.startswith('shared'):     # starts with shared drive path?
        app_log.debug(f"shared_drive_save: starts with shared")

        sub_path = path[6:]             # remove 'shared' from path

        if sub_path.startswith('/'):    # if has / at start, remove it
            sub_path = sub_path[1:]

        path = os.path.join("/mnt/shared/", sub_path)

    if '*' in path:                     # wildcard in path found?
        # try to find some .img file in that path, then update path
        pass

    if path:                            # got some path?
        if not os.path.exists(path):    # path does not exist?
            dialog(shared.main_loop, shared.current_body, f"The path does not exists: {path}")
            return

        if not os.path.isfile(path):    # path exists, but it's not a file?
            dialog(shared.main_loop, shared.current_body, f"The path is not a file: {path}")
            return

    if "/mnt/shared/" in path:          # path is pointing to shared drive?
        dialog(shared.main_loop, shared.current_body, "It is not safe to mount HDD Image from network.")

    settings_save()
    back_to_main_menu(None)
