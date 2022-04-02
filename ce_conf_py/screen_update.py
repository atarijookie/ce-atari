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
from screen_network import create_setting_row

app_log = logging.getLogger()


def update_create(button):
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
    button_up_online = create_my_button("OnlineUp", update_online)
    button_up_usb = create_my_button("  USB", update_usb)
    button_cancel = create_my_button(" Cancel", on_cancel)
    buttons = urwid.GridFlow([button_up_online, button_up_usb, button_cancel], 12, 0, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', col1w + col2w)
    shared.main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def update_online(button):
    pass


def update_usb(button):
    pass

