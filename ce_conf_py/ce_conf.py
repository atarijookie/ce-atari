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
    reserve_columns = 6


def create_my_button(text, on_clicked_fn, on_clicked_data=None):
    button = MyButton(text, on_clicked_fn, on_clicked_data)
    attrmap = urwid.AttrMap(button, None, focus_map='reversed')  # reversed on button focused
    attrmap.user_data = on_clicked_data
    return attrmap


def on_license_save(button):
    # TODO: save license here

    back_to_main_menu(button)


def on_screen_license_key(button):
    body = []
    body.append(urwid.AttrMap(urwid.Text('>> HW License <<', align='center'), 'reversed'))
    body.append(urwid.Divider())
    body.append(urwid.Text('Device is missing hardware license.'))
    body.append(urwid.Text('For more info see:'))
    body.append(urwid.Text('http://joo.kie.sk/cosmosex/license'))
    body.append(urwid.Divider())
    body.append(urwid.Text('Hardware serial number:'))
    body.append(urwid.Text('00000000000000000000000000'))
    body.append(urwid.Divider())
    body.append(urwid.Text('License key:'))

    # add search edit line
    widget_license = urwid.Edit("", edit_text="")
    body.append(urwid.AttrMap(widget_license, None, focus_map='reversed'))
    body.append(urwid.Divider())

    # add save + cancel button
    button_save = create_my_button(" Save", on_license_save)
    button_cancel = create_my_button("Cancel", back_to_main_menu)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    main.original_widget = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)


def on_screen_acsi_config(button):
    body = []
    body.append(urwid.AttrMap(urwid.Text('>> ACSI IDs config <<', align='center'), 'reversed'))
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

    main.original_widget = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)


def on_acsi_ids_save(button):
    # TODO: save IDs here
    back_to_main_menu(button)


def on_screen_translated(button):
    pass

def on_screen_hdd_image(button):
    pass

def on_screen_shared_drive(button):
    pass

def on_screen_floppy_config(button):
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
