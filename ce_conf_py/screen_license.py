import urwid
import logging
from urwid_helpers import create_my_button, create_header_footer, create_edit, dialog
from utils import settings_load, settings_save, on_cancel, back_to_main_menu, on_editline_changed
import shared

app_log = logging.getLogger()


def license_create(button):
    global settings, settings_changed
    settings_changed = {}                       # no settings have been changed
    settings_load()

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
    cols, _ = create_edit('LICENSE_KEY', 40, on_editline_changed)              # license number here
    body.append(cols)
    body.append(urwid.Divider())

    # add save + cancel button
    button_save = create_my_button(" Save", license_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)
    shared.main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def license_save(button):
    # TODO: save license here

    back_to_main_menu(button)

