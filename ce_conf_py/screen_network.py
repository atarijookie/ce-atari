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


def create_setting_row(label, what, value, col1w, col2w, reverse=False):
    if what == 'checkbox':      # for checkbox
        widget = MyCheckBox('', state=value)
        label = "   " + label
    elif what == 'edit':        # for edit line
        widget, _ = create_edit(value, col2w)
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


def network_create(button):
    global settings, settings_changed
    settings_changed = {}                       # no settings have been changed
    settings_load()

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
    button_save = create_my_button(" Save", network_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 36)
    shared.main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def network_save(button):
    pass

