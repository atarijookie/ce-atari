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


def shared_drive_create(button):
    global settings, settings_changed
    settings_changed = {}                       # no settings have been changed
    settings_load()

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
        ('fixed', 22, urwid.Text('NFS'))],
        dividechars=0)
    body.append(cols)

    cols = urwid.Columns([              # samba / cifs option row
        ('fixed', 10, urwid.Text('')),
        ('fixed', 7, b2),
        ('fixed', 22, urwid.Text('Samba / cifs / windows'))],
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
    button_save = create_my_button(" Save", shared_drive_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)
    shared.main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def shared_drive_save(button):
    pass

