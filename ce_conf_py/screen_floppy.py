from loguru import logger as app_log
import urwid
from urwid_helpers import create_my_button, create_header_footer, MyCheckBox, create_radio_button_options_rows
from utils import settings_load, settings_save, on_cancel, back_to_main_menu, setting_get_bool, \
    on_checkbox_changed
import shared


def floppy_create(button):
    settings_load()

    header, footer = create_header_footer('Floppy configuration')

    body = []
    body.append(urwid.Divider())

    col1w = 16
    colw = 5
    total_width = 23

    # enabling / disabling floppy
    btn_enabled = MyCheckBox('', on_state_change=on_enabled_changed)
    btn_enabled.set_state(setting_get_bool('FLOPPYCONF_ENABLED'))

    cols = urwid.Columns([
        ('fixed', col1w, urwid.Text('Floppy enabled')),
        ('fixed', colw, btn_enabled)],
        dividechars=0)

    body.append(cols)
    body.extend([urwid.Divider(), urwid.Divider()])

    # row with drive IDs - 0/1
    cols = create_radio_button_options_rows(
        col1w, "Drive ID",
        [{'value': 0, 'text': '0'},
         {'value': 1, 'text': '1'}],
        "FLOPPYCONF_DRIVEID", total_width)
    body.extend(cols)
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

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', total_width)
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
