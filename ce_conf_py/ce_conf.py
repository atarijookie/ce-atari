import copy
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
import logging
from logging.handlers import RotatingFileHandler
from urwid_helpers import create_edit_one, create_my_button, create_header_footer, create_edit, MyRadioButton, \
    MyCheckBox, dialog

setproctitle("ce_config")       # set process title
terminal_cols = 80  # should be 40 for ST low, 80 for ST mid
terminal_rows = 23

main_loop = None
current_body = None
should_run = True

log_formatter = logging.Formatter('%(asctime)s %(levelname)s %(funcName)s(%(lineno)d) %(message)s')

my_handler = RotatingFileHandler('/tmp/ce_conf.log', mode='a', maxBytes=1024 * 1024, backupCount=1, encoding=None,
                                 delay=0)
my_handler.setFormatter(log_formatter)
my_handler.setLevel(logging.DEBUG)

app_log = logging.getLogger('root')
app_log.setLevel(logging.DEBUG)
app_log.addHandler(my_handler)

settings_path = "/ce/settings"          # path to settings dir
settings = {}                           # current settings
settings_changed = {}                   # settings that changed and need to be saved
settings_default = {'DRIVELETTER_FIRST': 'C', 'DRIVELETTER_SHARED': 'P', 'DRIVELETTER_CONFDRIVE': 'O',
                    'MOUNT_RAW_NOT_TRANS': 0, 'SHARED_ENABLED': 0, 'SHARED_NFS_NOT_SAMBA': 0, 'FLOPPYCONF_ENABLED': 1,
                    'FLOPPYCONF_DRIVEID': 0, 'FLOPPYCONF_WRITEPROTECTED': 0, 'FLOPPYCONF_SOUND_ENABLED': 1,
                    'ACSI_DEVTYPE_0': 0, 'ACSI_DEVTYPE_1': 1, 'ACSI_DEVTYPE_2': 0, 'ACSI_DEVTYPE_3': 0,
                    'ACSI_DEVTYPE_4': 0, 'ACSI_DEVTYPE_5': 0, 'ACSI_DEVTYPE_6': 0, 'ACSI_DEVTYPE_7': 0,
                    'KEYBOARD_KEYS_JOY0': 'A%S%D%W%LSHIFT', 'KEYBOARD_KEYS_JOY1': 'LEFT%DOWN%RIGHT%UP%RSHIFT'}


def settings_load():
    """ load all the present settings from the settings dir """

    global settings
    settings = copy.deepcopy(settings_default)  # fill settings with default values before loading

    for f in os.listdir(settings_path):         # go through the settings dir
        path = os.path.join(settings_path, f)   # create full path

        if not os.path.isfile(path):            # if it's not a file, skip it
            continue

        with open(path, "r") as file:           # read the file into value in dictionary
            value = file.readline()
            settings[f] = value
            app_log.debug(f"settings_load: settings[{f}] = {value}")


def settings_save():
    """ save only changed settings to settings dir """
    for key, value in settings_changed.items():     # get all the settings that have changed
        path = os.path.join(settings_path, key)     # create full path

        with open(path, "w") as file:               # write to that file
            file.write(str(value))
            app_log.debug(f"settings_save: {key} -> {value}")


def alarm_callback(loop=None, data=None):
    """ this gets called on alarm """
    pass


def update_status(new_status):
    """ call this method to update status bar on screen """
    global main_loop

    if main_loop:  # if got main loop, trigger alarm to redraw widgets
        main_loop.set_alarm_in(1, alarm_callback)


def on_license_save(button):
    # TODO: save license here

    back_to_main_menu(button)


def on_screen_license_key(button):
    header, footer = create_header_footer('HW License')

    body = []
    body.append(urwid.Divider())
    body.append(urwid.Text('Device is missing hardware license.'))
    body.append(urwid.Text('For more info see:'))
    body.append(urwid.Text('http://joo.kie.sk/cosmosex/license'))
    body.append(urwid.Divider())
    body.append(urwid.Text('Hardware serial number:'))
    body.append(urwid.Text('00000000000000000000000000'))
    body.append(urwid.Divider())
    body.append(urwid.Text('License key:'))

    # add license edit line
    cols = create_edit('', 40)              # license number here
    body.append(cols)
    body.append(urwid.Divider())

    # add save + cancel button
    button_save = create_my_button(" Save", on_license_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)
    main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def on_screen_acsi_config(button):
    header, footer = create_header_footer('ACSI IDs config')

    body = []
    body.append(urwid.Divider())

    width = 6
    width_first = width + 2

    # add header row to all IDs config
    cols = urwid.Columns([
        ('fixed', width_first, urwid.Text("ID")),
        ('fixed', width, urwid.Text(" off")),
        ('fixed', width, urwid.Text(" sd")),
        ('fixed', width, urwid.Text(" raw")),
        ('fixed', width, urwid.Text("ce_dd"))],
        dividechars=0)

    body.append(cols)

    # fill in each row
    for id_ in range(8):
        id_str = urwid.Text(f" {id_}")          # ID number

        bgroup = []                             # button group
        b1 = MyRadioButton(bgroup, u'', on_state_change=on_acsi_id_changed, user_data={'id': id_, 'value': 0})  # off
        b2 = MyRadioButton(bgroup, u'', on_state_change=on_acsi_id_changed, user_data={'id': id_, 'value': 1})  # sd
        b3 = MyRadioButton(bgroup, u'', on_state_change=on_acsi_id_changed, user_data={'id': id_, 'value': 2})  # raw
        b4 = MyRadioButton(bgroup, u'', on_state_change=on_acsi_id_changed, user_data={'id': id_, 'value': 3})  # ce_dd

        # put items into Columns (== in a Row)
        cols = urwid.Columns([
            ('fixed', width_first, id_str),
            ('fixed', width, b1),
            ('fixed', width, b2),
            ('fixed', width, b3),
            ('fixed', width, b4)],
            dividechars=0)

        body.append(cols)

    body.append(urwid.Divider())

    # add save + cancel button
    button_save = create_my_button(" Save", on_acsi_ids_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    body.append(urwid.Divider())
    body.append(urwid.Divider())

    body.append(urwid.Text('off   - turned off, not responding here ', align='center'))
    body.append(urwid.Text('sd    - sd card (only one)              ', align='center'))
    body.append(urwid.Text('raw   - raw sector access (use HDDr/ICD)', align='center'))
    body.append(urwid.Text('ce_dd - for booting CE_DD driver        ', align='center'))

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)
    main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def on_acsi_id_changed(button, state, data):
    """ when ACSI ID setting changes from one value to another (e.g. off -> sd) """
    if not state:                   # called on the checkbox, which is now off? skip it
        return

    id_ = data['id']                        # get ID
    key = f'ACSI_DEVTYPE_{id_}'             # construct key name
    value = data['value']
    settings_changed[key] = value           # store value
    app_log.debug(f"on_acsi_id_changed: {key = } -> {value = }")


def on_acsi_ids_save(button):
    """ function that gets called on saving ACSI IDs config """

    global settings, settings_changed

    if not settings_changed:        # no settings changed, just quit
        back_to_main_menu(button)

    something_active = False
    count_sd = 0
    count_translated = 0

    id_types = {}
    for id_ in range(8):
        key = f'ACSI_DEVTYPE_{id_}'
        id_types[key] = settings.get(key, 0)        # get settings before change

        new_type = settings_changed.get(key)        # try to get the changed value

        if new_type is not None:                    # if value was found, store it
            id_types[key] = new_type

        if id_types[key] != 0:                      # if found something that is not OFF, got something active
            something_active = True

        if id_types[key] == 1:                      # found SD card type
            count_sd += 1

        if id_types[key] == 3:                      # found translated type
            count_translated += 1

    # after the loop validate settings, show warning
    if not something_active:
        dialog(main_loop, current_body, ["All ACSI/SCSI IDs are set to 'OFF',\n" 
                         "it is invalid and would brick the device.\n"
                         "Select at least one active ACSI/SCSI ID."])
        return

    if count_translated > 1:
        dialog(main_loop, current_body, ["You have more than 1 CE_DD selected.\r"
                         "Unselect some to leave only\n"
                         "1 active."])
        return

    if count_sd > 1:
        dialog(main_loop, current_body, "You have more than 1 SD cards\n"
                         "selected. Unselect some to leave only\n"
                         "1 active.")
        return

    # if(hwConfig.hddIface == HDD_IF_SCSI) {                         // running on SCSI? Show warning if ID 0 or 7 is used
    #if (devTypes[0] != DEVTYPE_OFF | | devTypes[7] != DEVTYPE_OFF) {
    #showMessageScreen("Warning", "You assigned something to ID 0 or ID 7.\n\rThey might not work as they might be\n\rused by SCSI controller.\n\r");
    #}

    settings_save()

    back_to_main_menu(button)


def on_screen_translated(button):
    header, footer = create_header_footer('Translated disk')

    body = []
    body.append(urwid.Divider())

    body.append(urwid.AttrMap(urwid.Text('Drive letters assignment', align='center'), 'reversed'))
    body.append(urwid.Divider())

    # helper function to create one translated drives letter config row
    def create_drive_row(label):
        col1 = 25
        col2 = 5

        edit_one = create_edit_one('')

        cols = urwid.Columns([
            ('fixed', col1, urwid.Text(label)),
            ('fixed', col2, edit_one)],
            dividechars=0)

        return cols

    # translated drive letter
    trans_first = create_drive_row("First translated drive")
    body.append(trans_first)
    body.append(urwid.Divider())

    # shared drive letter
    trans_shared = create_drive_row("Shared drive")
    body.append(trans_shared)

    # config drive letter
    trans_config = create_drive_row("Config drive")
    body.append(trans_config)

    body.append(urwid.Divider())
    body.append(urwid.AttrMap(urwid.Text('Options', align='center'), 'reversed'))
    body.append(urwid.Divider())

    def create_options_rows(label, option1, option2):
        bgroup = []  # button group
        b1 = MyRadioButton(bgroup, u'')  # option1 button
        b2 = MyRadioButton(bgroup, u'')  # option2 button

        cols1_ = urwid.Columns([
            ('fixed', 21, urwid.Text(label)),
            ('fixed', 6, b1),
            ('fixed', 10, urwid.Text(option1))],
            dividechars=0)

        cols2_ = urwid.Columns([
            ('fixed', 21, urwid.Text('')),
            ('fixed', 6, b2),
            ('fixed', 10, urwid.Text(option2))],
            dividechars=0)

        return cols1_, cols2_

    cols1, cols2 = create_options_rows("Mount USB media as", "translated", "raw")
    body.extend([cols1, cols2, urwid.Divider()])

    cols1, cols2 = create_options_rows("Access ZIP files as", "files", "dirs")
    body.extend([cols1, cols2, urwid.Divider()])

    # add save + cancel button
    button_save = create_my_button(" Save", on_translated_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    body.append(urwid.Divider())

    body.append(urwid.Text('If you use also raw disks (Atari native ', align='center'))
    body.append(urwid.Text('disks), you should avoid using few      ', align='center'))
    body.append(urwid.Text('letters from C: to leave some space for ', align='center'))
    body.append(urwid.Text('them.                                   ', align='center'))

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)
    main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def on_translated_save(button):
    pass


def on_screen_hdd_image(button):
    header, footer = create_header_footer('Disk image settings')

    body = []
    body.append(urwid.Divider())

    body.append(urwid.Text('HDD image path on RPi', align='left'))

    cols = create_edit('', 40)          # hdd image path here
    body.append(cols)

    body.append(urwid.Divider())

    # add save + cancel button
    button_save = create_my_button(" Save", on_hdd_img_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    button_clear = create_my_button("Clear", on_hdd_img_clear)
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
    main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def on_hdd_img_save(button):
    pass


def on_hdd_img_clear(button):
    pass


def on_screen_shared_drive(button):
    header, footer = create_header_footer('Shared drive settings')

    body = []
    body.append(urwid.Divider())

    body.append(urwid.Text('Define what folder on which machine will', align='center'))
    body.append(urwid.Text('be used as drive mounted through network', align='center'))
    body.append(urwid.Text('on CosmosEx. Works in translated mode.  ', align='center'))
    body.append(urwid.Divider())

    btn_enabled = MyCheckBox('')

    # enabling / disabling shared drive
    cols = urwid.Columns([
        ('fixed', 10, urwid.Text('Enabled', align='left')),
        ('fixed', 7, btn_enabled)],
        dividechars=0)
    body.append(cols)

    # shared drive protocol (NFS or samba)
    body.append(urwid.Divider())
    body.append(urwid.Text('Sharing protocol', align='left'))

    bgrp = []  # button group
    b1 = MyRadioButton(bgrp, u'')       # NFS
    b2 = MyRadioButton(bgrp, u'')       # samba / cifs

    cols = urwid.Columns([              # NFS option row
        ('fixed', 10, urwid.Text('')),
        ('fixed', 7, b1),
        ('fixed', 25, urwid.Text('NFS'))],
        dividechars=0)
    body.append(cols)

    cols = urwid.Columns([              # samba / cifs option row
        ('fixed', 10, urwid.Text('')),
        ('fixed', 7, b2),
        ('fixed', 25, urwid.Text('Samba / cifs / windows'))],
        dividechars=0)
    body.append(cols)
    body.append(urwid.Divider())

    # IP of machine sharing info
    body.append(urwid.Text('IP address of server', align='left'))

    cols_edit_ip = create_edit('', 17)
    cols = urwid.Columns([              # NFS option row
        ('fixed', 10, urwid.Text('')),
        ('fixed', 17, cols_edit_ip)],
        dividechars=0)
    body.append(cols)
    body.append(urwid.Divider())

    # folder on sharing machine
    body.append(urwid.Text('Shared folder path on server', align='left'))

    cols_edit_path = create_edit('', 40)
    body.append(cols_edit_path)
    body.append(urwid.Divider())

    # username and password
    cols_edit_username = create_edit('', 20)
    cols = urwid.Columns([
        ('fixed', 10, urwid.Text('Username', align='left')),
        ('fixed', 20, cols_edit_username)],
        dividechars=0)
    body.append(cols)

    cols_edit_password = create_edit('', 20)
    cols = urwid.Columns([
        ('fixed', 10, urwid.Text('Password', align='left')),
        ('fixed', 20, cols_edit_password)],
        dividechars=0)
    body.append(cols)
    body.append(urwid.Divider())

    # add save + cancel button
    button_save = create_my_button(" Save", on_shared_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)
    main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def on_shared_save(button):
    pass


def on_screen_floppy_config(button):
    header, footer = create_header_footer('Floppy configuration')

    body = []
    body.append(urwid.Divider())

    col1w = 17
    colw = 5

    # enabling / disabling floppy
    btn_enabled = MyCheckBox('')
    cols = urwid.Columns([
        ('fixed', col1w, urwid.Text('Floppy enabled')),
        ('fixed', colw, btn_enabled)],
        dividechars=0)

    body.append(cols)
    body.append(urwid.Divider())

    # row with drive IDs - 0/1
    cols = urwid.Columns([
        ('fixed', col1w-2, urwid.Text('')),
        ('fixed', colw, urwid.Text('0', align=urwid.CENTER)),
        ('fixed', colw, urwid.Text('1', align=urwid.CENTER))],
        dividechars=2)
    body.append(cols)

    # drive ID selection row
    bgrp = []  # button group
    b1 = MyRadioButton(bgrp, u'')       # drive ID 0
    b2 = MyRadioButton(bgrp, u'')       # drive ID 1

    cols = urwid.Columns([
        ('fixed', col1w-2, urwid.Text('Drive ID')),
        ('fixed', colw, b1),               # drive ID 0
        ('fixed', colw, b2)],              # drive ID 1
        dividechars=2)
    body.append(cols)
    body.extend([urwid.Divider(), urwid.Divider()])

    # write protected floppy
    btn_write_protect = MyCheckBox('')
    cols = urwid.Columns([
        ('fixed', col1w, urwid.Text('Write protected')),
        ('fixed', colw, btn_write_protect)],
        dividechars=0)
    body.append(cols)
    body.extend([urwid.Divider(), urwid.Divider()])

    # make seek sound checkbox
    btn_make_sound = MyCheckBox('')
    cols = urwid.Columns([
        ('fixed', col1w, urwid.Text('Make seek sound')),
        ('fixed', colw, btn_make_sound)],
        dividechars=0)
    body.append(cols)
    body.extend([urwid.Divider(), urwid.Divider()])

    # add save + cancel button
    button_save = create_my_button(" Save", on_floppy_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 30)
    main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def on_floppy_save(button):
    pass


def ikbd_checkbox_line(label, hint):
    body = []

    checkbox = MyCheckBox('')

    # first line with label and checkbox
    cols = urwid.Columns([
        ('fixed', 31, urwid.Text(label)),
        ('fixed', 5, checkbox)],
        dividechars=0)
    body.append(cols)

    # second line with hint
    if hint:
        body.append(urwid.Text(hint))

    # third line with divider
    body.append(urwid.Divider())

    return body


def ikbd_keyboard_joystick():
    body = []

    w_edit = 12
    cols_btn = create_edit('', w_edit)
    cols_up = create_edit('', w_edit)
    cols_left = create_edit('', w_edit)
    cols_down = create_edit('', w_edit)
    cols_right = create_edit('', w_edit)

    # button key and up key
    cols = urwid.Columns([
        ('fixed', w_edit, cols_btn),
        ('fixed', w_edit, cols_up),
        ('fixed', w_edit, urwid.Text(''))],
        dividechars=0)
    body.append(cols)

    # button left, down, right
    cols = urwid.Columns([
        ('fixed', w_edit, cols_left),
        ('fixed', w_edit, cols_down),
        ('fixed', w_edit, cols_right)],
        dividechars=0)
    body.append(cols)

    # divider
    body.append(urwid.Divider())

    return body


def on_screen_ikbd(button):
    header, footer = create_header_footer('IKBD settings')

    body = []
    body.append(urwid.Divider())

    # attach 1st joy as JOY 0
    chb_line = ikbd_checkbox_line('Attach 1st joy as JOY 0', '(hotkey: CTRL+any SHIFT+HELP/F11)')
    body.extend(chb_line)

    # mouse wheel as arrow up / down
    chb_line = ikbd_checkbox_line('Mouse wheel as arrow UP / DOWN', '')
    body.extend(chb_line)

    # Keyboard Joy 0 enabled
    body.append(urwid.Padding(urwid.AttrMap(urwid.Text(''), 'reversed'), 'center', 40))     # inverse divider line
    chb_line = ikbd_checkbox_line('Keyboard Joy 0 enabled', '(hotkey: CTRL+LSHIFT+UNDO/F12)')
    body.extend(chb_line)

    # keyboard buttons for joy 0
    cols_joy0 = ikbd_keyboard_joystick()
    body.extend(cols_joy0)

    # Keyboard Joy 1 enabled
    body.append(urwid.Padding(urwid.AttrMap(urwid.Text(''), 'reversed'), 'center', 40))     # inverse divider line
    chb_line = ikbd_checkbox_line('Keyboard Joy 1 enabled', '(hotkey: CTRL+RSHIFT+UNDO/F12)')
    body.extend(chb_line)

    # keyboard buttons for joy 1
    cols_joy1 = ikbd_keyboard_joystick()
    body.extend(cols_joy1)

    # add save + cancel button
    button_save = create_my_button(" Save", on_ikbd_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 36)
    main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def on_ikbd_save(button):
    pass


def create_setting_row(label, what, value, col1w, col2w, reverse=False):
    if what == 'checkbox':      # for checkbox
        widget = MyCheckBox('', state=value)
        label = "   " + label
    elif what == 'edit':        # for edit line
        widget = create_edit(value, col2w)
        label = "   " + label
    elif what == 'text':
        widget = urwid.Text(value)

        if reverse:     # if should be reversed, apply attrmap
            widget = urwid.AttrMap(widget, 'reversed')
    else:                       # for title
        widget = urwid.Text('')

    # create label text
    text_label = urwid.Text(label)

    if reverse:         # if should be reversed, apply attrmap
        text_label = urwid.AttrMap(text_label, 'reversed')

    # put things into columns
    cols = urwid.Columns([
        ('fixed', col1w, text_label),
        ('fixed', col2w, widget)],
        dividechars=0)

    return cols


def on_screen_network_settings(button):
    header, footer = create_header_footer('Network settings')

    body = []
    body.append(urwid.Divider())

    col1w = 16
    col2w = 17

    # hostname and DNS
    cols = create_setting_row('Hostname', 'edit', '', col1w, col2w)
    body.append(cols)

    cols = create_setting_row('DNS', 'edit', '', col1w, col2w)
    body.append(cols)
    body.append(urwid.Divider())

    # ethernet settings
    cols = create_setting_row('Ethernet', 'title', '', col1w, col2w)
    body.append(cols)

    cols = create_setting_row('Use DHCP', 'checkbox', False, col1w, col2w)
    body.append(cols)

    cols = create_setting_row('IP address', 'edit', '', col1w, col2w)
    body.append(cols)

    cols = create_setting_row('Mask', 'edit', '', col1w, col2w)
    body.append(cols)

    cols = create_setting_row('Gateway', 'edit', '', col1w, col2w)
    body.append(cols)
    body.append(urwid.Divider())

    # wifi settings
    cols = create_setting_row('Wifi', 'title', '', col1w, col2w)
    body.append(cols)

    cols = create_setting_row('Enable', 'checkbox', False, col1w, col2w)
    body.append(cols)

    cols = create_setting_row('WPA SSID', 'edit', '', col1w, col2w)
    body.append(cols)

    cols = create_setting_row('WPA PSK', 'edit', '', col1w, col2w)
    body.append(cols)
    body.append(urwid.Divider())

    cols = create_setting_row('Use DHCP', 'checkbox', False, col1w, col2w)
    body.append(cols)

    cols = create_setting_row('IP address', 'edit', '', col1w, col2w)
    body.append(cols)

    cols = create_setting_row('Mask', 'edit', '', col1w, col2w)
    body.append(cols)

    cols = create_setting_row('Gateway', 'edit', '', col1w, col2w)
    body.append(cols)
    body.append(urwid.Divider())

    # add save + cancel button
    button_save = create_my_button(" Save", on_network_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 36)
    main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def on_network_save(button):
    pass


def on_screen_update(button):
    header, footer = create_header_footer('Software & Firmware updates')

    body = []
    body.append(urwid.Divider())

    col1w = 24
    col2w = 12

    cols = create_setting_row('Hardware version  :', 'text', '', col1w, col2w)
    body.append(cols)

    cols = create_setting_row('HDD interface type:', 'text', '', col1w, col2w)
    body.append(cols)

    cols = create_setting_row('RPi revision      :', 'text', '', col1w, col2w)
    body.append(cols)
    body.append(urwid.Divider())

    cols = create_setting_row('part', 'text', 'version', col1w, col2w, reverse=True)
    body.append(cols)

    cols = create_setting_row('Main App', 'text', '', col1w, col2w)
    body.append(cols)

    cols = create_setting_row('Horst', 'text', '', col1w, col2w)
    body.append(cols)
    body.append(urwid.Divider())

    cols = create_setting_row('Status', 'text', 'unknown', col1w, col2w, reverse=True)
    body.append(cols)
    body.append(urwid.Divider())

    # add update + cancel buttons
    button_up_online = create_my_button("OnlineUp", on_update_online)
    button_up_usb = create_my_button("  USB", on_update_usb)
    button_cancel = create_my_button(" Cancel", on_cancel)
    buttons = urwid.GridFlow([button_up_online, button_up_usb, button_cancel], 12, 0, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', col1w + col2w)
    main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def on_update_online(button):
    pass


def on_update_usb(button):
    pass


def create_main_menu():
    body = []
    header, footer = create_header_footer('CE Config - main menu')

    body.append(urwid.Divider())

    menu_items = [('! License key !', on_screen_license_key), ('ACSI IDs', on_screen_acsi_config),
                  ('Translated disks', on_screen_translated), ('Hard Disk Image', on_screen_hdd_image),
                  ('Shared drive', on_screen_shared_drive), ('Floppy config', on_screen_floppy_config),
                  ('Network settings', on_screen_network_settings), ('IKBD', on_screen_ikbd),
                  ('Update software', on_screen_update)]

    # create main menu buttons
    for btn_text, handler in menu_items:
        button = create_my_button(btn_text, handler)
        button = urwid.Padding(button, 'center', 20)        # center buttons, exact width 20 chars
        body.append(button)
        body.append(urwid.Divider())

    body.append(urwid.Divider())

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)
    return urwid.Frame(w_body, header=header, footer=footer)


def show_no_storage():
    """ show warning message that no storage is found """
    body = []

    body.append(urwid.Text("Failed to get storage path."))
    body.append(urwid.Divider())
    body.append(urwid.Text("Attach USB drive or shared network drive and try again."))
    body.append(urwid.Divider())

    btn = create_my_button("Back", back_to_main_menu)
    body.append(btn)

    main.original_widget = urwid.Filler(urwid.Pile(body))


def on_cancel(button):
    global settings_changed
    settings_changed = {}       # no settings have been changed
    back_to_main_menu(None)     # return to main menu


def back_to_main_menu(button):
    """ when we should return back to main menu """
    main.original_widget = urwid.Padding(create_main_menu(), left=2, right=2)


def exit_program(button):
    raise urwid.ExitMainLoop()


# the config tool execution starts here
terminal_cols, terminal_rows = urwid.raw_display.Screen().get_cols_rows()
items_per_page = terminal_rows - 4

main = urwid.Padding(create_main_menu(), left=2, right=2)

top = urwid.Overlay(main, urwid.SolidFill(),
                    align='center', width=('relative', 100),
                    valign='middle', height=('relative', 100),
                    min_width=20, min_height=9)
current_body = top

settings_load()     # load all the settings

try:
    main_loop = urwid.MainLoop(top, palette=[('reversed', 'standout', '')])
    main_loop.run()
except KeyboardInterrupt:
    print("Terminated by keyboard...")

should_run = False
