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

setproctitle("ce_config")       # set process title
terminal_cols = 80  # should be 40 for ST low, 80 for ST mid
terminal_rows = 23

should_run = True

log_formatter = logging.Formatter('%(asctime)s %(levelname)s %(funcName)s(%(lineno)d) %(message)s')

my_handler = RotatingFileHandler('/tmp/ce_downloader.log', mode='a', maxBytes=1024 * 1024, backupCount=1, encoding=None,
                                 delay=0)
my_handler.setFormatter(log_formatter)
my_handler.setLevel(logging.DEBUG)

app_log = logging.getLogger('root')
app_log.setLevel(logging.DEBUG)
app_log.addHandler(my_handler)


def alarm_callback(loop=None, data=None):
    """ this gets called on alarm """
    pass


def update_status(new_status):
    """ call this method to update status bar on screen """
    global main_loop

    if main_loop:  # if got main loop, trigger alarm to redraw widgets
        main_loop.set_alarm_in(1, alarm_callback)


class ButtonLabel(urwid.SelectableIcon):
    """ to hide curson on button, move cursor position outside of button
    """

    def set_text(self, label):
        self.__super.set_text(label)
        self._cursor_position = len(label) + 1  # move cursor position outside of button


class MyButton(urwid.Button):
    button_left = "["
    button_right = "]"

    def __init__(self, label, on_press=None, user_data=None):
        self._label = ButtonLabel("")
        self.user_data = user_data

        cols = urwid.Columns([
            ('fixed', len(self.button_left), urwid.Text(self.button_left)),
            self._label,
            ('fixed', len(self.button_right), urwid.Text(self.button_right))],
            dividechars=1)
        super(urwid.Button, self).__init__(cols)

        if on_press:
            urwid.connect_signal(self, 'click', on_press, user_data)

        self.set_label(label)


class MyRadioButton(urwid.RadioButton):
    states = {
        True: urwid.SelectableIcon("[ * ]", 2),
        False: urwid.SelectableIcon("[   ]", 2),
        'mixed': urwid.SelectableIcon("[ # ]", 2)
    }
    reserve_columns = 5


class MyCheckBox(urwid.CheckBox):
    states = {
        True: urwid.SelectableIcon("[ * ]", 2),
        False: urwid.SelectableIcon("[   ]", 2),
        'mixed': urwid.SelectableIcon("[ # ]", 2)
    }
    reserve_columns = 5


class EditOne(urwid.Text):
    _selectable = True
    ignore_focus = False
    # (this variable is picked up by the MetaSignals metaclass)
    signals = ["change", "postchange"]

    def __init__(self, edit_text):
        super().__init__(markup=edit_text)

    def valid_char(self, ch):
        if len(ch) != 1:
            return False

        och = ord(ch)
        return (65 <= och <= 90) or (97 <= och <= 122)

    def keypress(self, size, key):
        if self.valid_char(key):        # valid key, use it
            new_text = str(key).upper()
            self.set_text(new_text)
        else:                           # key wasn't handled
            return key


def create_edit_one(edit_text):
    edit_one = EditOne(edit_text)
    edit_one_decorated = urwid.AttrMap(edit_one, None, focus_map='reversed')

    cols = urwid.Columns([
        ('fixed', 2, urwid.Text('[ ')),
        ('fixed', 1, edit_one_decorated),
        ('fixed', 2, urwid.Text(' ]'))],
        dividechars=0)

    return cols


def create_my_button(text, on_clicked_fn, on_clicked_data=None):
    button = MyButton(text, on_clicked_fn, on_clicked_data)
    attrmap = urwid.AttrMap(button, None, focus_map='reversed')  # reversed on button focused
    attrmap.user_data = on_clicked_data
    return attrmap


def create_header_footer(header_text, footer_text=None):
    header = urwid.AttrMap(urwid.Text(header_text, align='center'), 'reversed')
    header = urwid.Padding(header, 'center', 40)

    if footer_text is None:
        footer_text = 'F5 - refresh, Ctrl+C or F10 - quit'

    footer = urwid.AttrMap(urwid.Text(footer_text, align='center'), 'reversed')
    footer = urwid.Padding(footer, 'center', 40)

    return header, footer


def create_edit(text, width):
    edit_line = urwid.Edit(caption="", edit_text=text)
    edit_decorated = urwid.AttrMap(edit_line, None, focus_map='reversed')

    cols = urwid.Columns([
        ('fixed', 1, urwid.Text('[')),
        ('fixed', width - 2, edit_decorated),
        ('fixed', 1, urwid.Text(']'))],
        dividechars=0)

    return cols


def on_license_save(button):
    # TODO: save license here

    back_to_main_menu(button)


def on_screen_license_key(button):
    header, footer = create_header_footer('>> HW License <<')

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
    cols = create_edit("", 40)              # license number here
    body.append(cols)
    body.append(urwid.Divider())

    # add save + cancel button
    button_save = create_my_button(" Save", on_license_save)
    button_cancel = create_my_button("Cancel", back_to_main_menu)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)
    main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def on_screen_acsi_config(button):
    header, footer = create_header_footer('>> ACSI IDs config <<')

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
        id_str = urwid.Text(f" {id_}")           # ID number

        bgroup = []                             # button group
        b1 = MyRadioButton(bgroup, u"")         # off
        b2 = MyRadioButton(bgroup, u"")         # sd
        b3 = MyRadioButton(bgroup, u"")         # raw
        b4 = MyRadioButton(bgroup, u"")         # ce_dd

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
    button_cancel = create_my_button("Cancel", back_to_main_menu)
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


def on_acsi_ids_save(button):
    # TODO: save IDs here
    back_to_main_menu(button)


def on_screen_translated(button):
    header, footer = create_header_footer('>> Translated disk <<')

    body = []
    body.append(urwid.Divider())

    body.append(urwid.AttrMap(urwid.Text('Drive letters assignment', align='center'), 'reversed'))
    body.append(urwid.Divider())

    # helper function to create one translated drives letter config row
    def create_drive_row(label):
        col1 = 25
        col2 = 5

        edit_one = create_edit_one("")

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
        b1 = MyRadioButton(bgroup, u"")  # option1 button
        b2 = MyRadioButton(bgroup, u"")  # option2 button

        cols1_ = urwid.Columns([
            ('fixed', 21, urwid.Text(label)),
            ('fixed', 6, b1),
            ('fixed', 10, urwid.Text(option1))],
            dividechars=0)

        cols2_ = urwid.Columns([
            ('fixed', 21, urwid.Text("")),
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
    button_cancel = create_my_button("Cancel", back_to_main_menu)
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
    header, footer = create_header_footer('>> Disk image settings <<')

    body = []
    body.append(urwid.Divider())

    body.append(urwid.Text('HDD image path on RPi', align='left'))

    cols = create_edit("", 40)          # hdd image path here
    body.append(cols)

    body.append(urwid.Divider())

    # add save + cancel button
    button_save = create_my_button(" Save", on_hdd_img_save)
    button_cancel = create_my_button("Cancel", back_to_main_menu)
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
    header, footer = create_header_footer('>> Shared drive settings <<')

    body = []
    body.append(urwid.Divider())

    body.append(urwid.Text('Define what folder on which machine will', align='center'))
    body.append(urwid.Text('be used as drive mounted through network', align='center'))
    body.append(urwid.Text('on CosmosEx. Works in translated mode.  ', align='center'))
    body.append(urwid.Divider())

    btn_enabled = MyCheckBox("")

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
    b1 = MyRadioButton(bgrp, u"")       # NFS
    b2 = MyRadioButton(bgrp, u"")       # samba / cifs

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

    cols_edit_ip = create_edit("", 17)
    cols = urwid.Columns([              # NFS option row
        ('fixed', 10, urwid.Text('')),
        ('fixed', 17, cols_edit_ip)],
        dividechars=0)
    body.append(cols)
    body.append(urwid.Divider())

    # folder on sharing machine
    body.append(urwid.Text('Shared folder path on server', align='left'))

    cols_edit_path = create_edit("", 40)
    body.append(cols_edit_path)
    body.append(urwid.Divider())

    # username and password
    cols_edit_username = create_edit("", 20)
    cols = urwid.Columns([
        ('fixed', 10, urwid.Text('Username', align='left')),
        ('fixed', 20, cols_edit_username)],
        dividechars=0)
    body.append(cols)

    cols_edit_password = create_edit("", 20)
    cols = urwid.Columns([
        ('fixed', 10, urwid.Text('Password', align='left')),
        ('fixed', 20, cols_edit_password)],
        dividechars=0)
    body.append(cols)
    body.append(urwid.Divider())

    # add save + cancel button
    button_save = create_my_button(" Save", on_shared_save)
    button_cancel = create_my_button("Cancel", back_to_main_menu)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)
    main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def on_shared_save(button):
    pass


def on_screen_floppy_config(button):
    header, footer = create_header_footer('>> Floppy configuration <<')

    body = []
    body.append(urwid.Divider())

    col1w = 17
    colw = 5

    # enabling / disabling floppy
    btn_enabled = MyCheckBox("")
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
    b1 = MyRadioButton(bgrp, u"")       # drive ID 0
    b2 = MyRadioButton(bgrp, u"")       # drive ID 1

    cols = urwid.Columns([
        ('fixed', col1w-2, urwid.Text('Drive ID')),
        ('fixed', colw, b1),               # drive ID 0
        ('fixed', colw, b2)],              # drive ID 1
        dividechars=2)
    body.append(cols)
    body.extend([urwid.Divider(), urwid.Divider()])

    # write protected floppy
    btn_write_protect = MyCheckBox("")
    cols = urwid.Columns([
        ('fixed', col1w, urwid.Text('Write protected')),
        ('fixed', colw, btn_write_protect)],
        dividechars=0)
    body.append(cols)
    body.extend([urwid.Divider(), urwid.Divider()])

    # make seek sound checkbox
    btn_make_sound = MyCheckBox("")
    cols = urwid.Columns([
        ('fixed', col1w, urwid.Text('Make seek sound')),
        ('fixed', colw, btn_make_sound)],
        dividechars=0)
    body.append(cols)
    body.extend([urwid.Divider(), urwid.Divider()])

    # add save + cancel button
    button_save = create_my_button(" Save", on_floppy_save)
    button_cancel = create_my_button("Cancel", back_to_main_menu)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 30)
    main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def on_floppy_save(button):
    pass


def on_screen_network_settings(button):
    pass


def on_screen_ikbd(button):
    pass


def on_screen_update(button):
    pass


def create_main_menu():
    body = []
    body.append(urwid.AttrMap(urwid.Text('>> CE Config - main menu <<', align='center'), 'reversed'))
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
    return urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)


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


def back_to_main_menu(button):
    """ when we should return back to main menu """
    main.original_widget = urwid.Padding(create_main_menu(), left=2, right=2)


def exit_program(button):
    raise urwid.ExitMainLoop()


terminal_cols, terminal_rows = urwid.raw_display.Screen().get_cols_rows()
items_per_page = terminal_rows - 4

main = urwid.Padding(create_main_menu(), left=2, right=2)

top = urwid.Overlay(main, urwid.SolidFill(),
                    align='center', width=('relative', 100),
                    valign='middle', height=('relative', 100),
                    min_width=20, min_height=9)

try:
    main_loop = urwid.MainLoop(top, palette=[('reversed', 'standout', '')])
    main_loop.run()
except KeyboardInterrupt:
    print("Terminated by keyboard...")

should_run = False
