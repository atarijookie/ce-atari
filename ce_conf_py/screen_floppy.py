import logging
import urwid
from urwid_helpers import create_edit_one, create_my_button, create_header_footer, create_edit, MyRadioButton, \
    MyCheckBox, dialog
from utils import settings_load, settings_save, on_cancel, back_to_main_menu
import shared

app_log = logging.getLogger()


def floppy_create(button):
    settings_load()

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
    button_save = create_my_button(" Save", floppy_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 30)
    shared.main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def floppy_save(button):
    pass

