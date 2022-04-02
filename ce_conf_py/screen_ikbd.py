import copy
import os
import re
import urwid
from setproctitle import setproctitle
import logging
from logging.handlers import RotatingFileHandler
from urwid_helpers import create_edit_one, create_my_button, create_header_footer, create_edit, MyRadioButton, \
    MyCheckBox, dialog
from utils import settings_load, settings_save, on_cancel, back_to_main_menu
import shared

app_log = logging.getLogger()


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


def ikbd_create(button):
    global settings, settings_changed
    settings_changed = {}                       # no settings have been changed
    settings_load()

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
    button_save = create_my_button(" Save", ikbd_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 36)
    shared.main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def ikbd_save(button):
    pass

