import logging
import urwid
from urwid_helpers import create_my_button, create_header_footer, MyRadioButton, MyCheckBox, dialog
from utils import settings_load, settings_save, on_cancel, back_to_main_menu, setting_get_bool, on_option_changed, \
    on_checkbox_changed
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
    btn_enabled = MyCheckBox('', on_state_change=on_enabled_changed)
    btn_enabled.set_state(setting_get_bool('FLOPPYCONF_ENABLED'))

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
    b1 = MyRadioButton(
            bgrp, u'', on_state_change=on_option_changed,
            user_data={'id': 'FLOPPYCONF_DRIVEID', 'value': 0})       # drive ID 0

    b2 = MyRadioButton(
            bgrp, u'', on_state_change=on_option_changed,
            user_data={'id': 'FLOPPYCONF_DRIVEID', 'value': 1})       # drive ID 1

    value = setting_get_bool('FLOPPYCONF_DRIVEID')

    if not value:   # ID 0?
        b1.set_state(True)
    else:           # ID 1?
        b2.set_state(True)

    cols = urwid.Columns([
        ('fixed', col1w-2, urwid.Text('Drive ID')),
        ('fixed', colw, b1),               # drive ID 0
        ('fixed', colw, b2)],              # drive ID 1
        dividechars=2)
    body.append(cols)
    body.extend([urwid.Divider(), urwid.Divider()])

    # write protected floppy
    btn_write_protect = MyCheckBox('', on_state_change=on_writeprotected_changed)
    btn_write_protect.set_state(setting_get_bool('FLOPPYCONF_WRITEPROTECTED'))

    cols = urwid.Columns([
        ('fixed', col1w, urwid.Text('Write protected')),
        ('fixed', colw, btn_write_protect)],
        dividechars=0)
    body.append(cols)
    body.extend([urwid.Divider(), urwid.Divider()])

    # make seek sound checkbox
    btn_make_sound = MyCheckBox('', on_state_change=on_sound_changed)
    btn_make_sound.set_state(setting_get_bool('FLOPPYCONF_SOUND_ENABLED'))

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


def on_enabled_changed(button, state):
    on_checkbox_changed('FLOPPYCONF_ENABLED', state)


def on_writeprotected_changed(button, state):
    on_checkbox_changed('FLOPPYCONF_WRITEPROTECTED', state)


def on_sound_changed(button, state):
    on_checkbox_changed('FLOPPYCONF_SOUND_ENABLED', state)


def floppy_save(button):
    app_log.debug(f"floppy_save: {shared.settings_changed}")

    settings_save()
    back_to_main_menu(None)
