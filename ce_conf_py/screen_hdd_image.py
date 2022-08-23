import os
import re
import subprocess
import glob
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


def find_usb_or_shared(path_in):
    app_log.debug(f"find_usb_or_shared: starts with usb or shared: {path_in}")

    path_is_usb = path_in.startswith('usb')  # if True, starts with usb, if False starts with shared

    result = subprocess.run(['mount'], stdout=subprocess.PIPE)  # run mount command, ger output
    output = result.stdout.decode('utf-8')  # get output to var
    mounts = re.findall("(/mnt/\S*)", output)  # find anything starts with /mnt/

    offset = 3 if path_is_usb else 6  # how much we need to cut of - 3 for 'usb', 6 for 'shared'
    sub_path = path_in[offset:]  # get just the part after 'usb' / 'shared' start

    # if the sub-path starts with '/' we must remove it, otherwise it would resolve to root of filesystem
    if sub_path.startswith('/'):
        sub_path = sub_path[1:]

    app_log.debug(f"path_is_usb: {path_is_usb}")
    app_log.debug(f"mounts: {mounts}")
    app_log.debug(f"sub_path: {sub_path}")

    # go through mounts
    for mount in mounts:
        mount_is_shared = '/mnt/shared' in mount  # if True, this mount is for shared folder

        # if we're looking for usb/shared drive, but the mount is shared/usb, skip it
        if (path_is_usb and mount_is_shared) or (not path_is_usb and not mount_is_shared):
            app_log.debug(f"path_is_usb: {path_is_usb}, mount_is_shared: {mount_is_shared} -- ignoring")
            continue

        path = os.path.join(mount, sub_path)  # create full path
        app_log.debug(f"mount: {mount} + sub_path: {sub_path} = {path}")

        if os.path.exists(path) and os.path.isfile(path):   # if path exists, return it
            app_log.debug(f"path exists: {path}")
            return path

        app_log.debug(f"path does not exist: {path}")

        # let's check if we have wildcard in the path and then possibly try to resolve it
        if '*' in path:
            app_log.debug(f"path has wildcard that we will resolve: {path}")
            path2 = find_path_with_wildcard(path)

            if path2:     # found something existing, return it
                return path2

    return None     # no existing path found


def find_path_with_wildcard(path_in):
    app_log.debug("find_path_with_wildcard: will try to find pattern: {}".format(path_in))
    matches = glob.glob(path_in)  # try unix style pathname pattern expansion

    for match in matches:  # go through all the found matches
        if os.path.exists(match) and os.path.isfile(match):
            return match

    return None


def hdd_img_save(button):
    app_log.debug(f"shared_drive_save: {shared.settings_changed}")

    path_image = setting_get_str('HDDIMAGE')

    # if clear path is specified (user wants to remove hdd image), just save it
    if not path_image:
        settings_save()
        back_to_main_menu(None)
        return

    path_out = None

    if path_image.startswith('usb') or path_image.startswith('shared'):     # starts with usb or shared?
        path_out = find_usb_or_shared(path_image)
    elif '*' in path_image:                 # wildcard in path found?
        path_out = find_path_with_wildcard(path_image)
    else:       # not usb/shared path, and no wildcard? just check if it exists
        if os.path.exists(path_image) and os.path.isfile(path_image):   # path exists and it's a file? use it
            path_out = path_image

    # no valid path was found? show message and don't save
    if not path_out:
        dialog(shared.main_loop, shared.current_body, f"The path is invalid:\n{path_image}")
        return

    # path is pointing to shared drive? warn user
    if "/mnt/shared/" in path_out:
        dialog(shared.main_loop, shared.current_body, "It is not safe to mount HDD Image from network.")

    # store the new path to changed settings, save and return
    shared.settings_changed['HDDIMAGE'] = path_out
    settings_save()
    back_to_main_menu(None)
