import urwid
import logging
from urwid_helpers import create_edit_one, create_my_button, create_header_footer, MyRadioButton, dialog
from utils import settings_load, settings_save, on_cancel, back_to_main_menu, setting_get_merged, setting_get_bool, \
    on_option_changed
import shared

app_log = logging.getLogger()


def translated_create(button):
    settings_load()

    header, footer = create_header_footer('Translated disk')

    body = []
    body.append(urwid.AttrMap(urwid.Text('Drive letters assignment', align='center'), 'reversed'))

    # helper function to create one translated drives letter config row
    def create_drive_row(label, setting_name):
        col1 = 25
        col2 = 5
        pad = (40 - (col1 + col2)) // 2

        edit_one = create_edit_one(setting_name, on_edit_changed)

        cols = urwid.Columns([
            ('fixed', pad, urwid.Text(' ')),
            ('fixed', col1, urwid.Text(label)),
            ('fixed', col2, edit_one)],
            dividechars=0)

        return cols

    def add_drive_letter_row(body_list, label, setting_name):
        trans_row = create_drive_row(label, setting_name)
        body_list.append(trans_row)
        body_list.append(urwid.Divider())

    # translated drive letter
    body.append(urwid.Divider())

    add_drive_letter_row(body, 'First translated drive', 'DRIVELETTER_FIRST')
    add_drive_letter_row(body, 'Config drive', 'DRIVELETTER_CONFDRIVE')
    add_drive_letter_row(body, 'Shared drive', 'DRIVELETTER_SHARED')
    add_drive_letter_row(body, 'ZIP file drive', 'DRIVELETTER_ZIP')

    body.append(urwid.AttrMap(urwid.Text('Options', align='center'), 'reversed'))
    body.append(urwid.Divider())

    def create_options_rows(label, option_false, option_true, setting_name):
        value = setting_get_bool(setting_name)

        bgroup = []  # button group
        b1 = MyRadioButton(
            bgroup, '', on_state_change=on_option_changed,
            user_data={'id': setting_name, 'value': 1})         # option1 button

        b2 = MyRadioButton(
            bgroup, '', on_state_change=on_option_changed,
            user_data={'id': setting_name, 'value': 0})         # option2 button

        if value:               # 1st option should be selected?
            b1.set_state(True)
        else:                   # 2nd option should be selected?
            b2.set_state(True)

        cols1_ = urwid.Columns([
            ('fixed', 21, urwid.Text(label)),
            ('fixed', 6, b2),
            ('fixed', 10, urwid.Text(option_false))],
            dividechars=0)

        cols2_ = urwid.Columns([
            ('fixed', 21, urwid.Text('')),
            ('fixed', 6, b1),
            ('fixed', 10, urwid.Text(option_true))],
            dividechars=0)

        return cols1_, cols2_

    cols1, cols2 = create_options_rows("Mount USB media as", "translated", "raw", 'MOUNT_RAW_NOT_TRANS')
    body.extend([cols1, cols2, urwid.Divider()])

    # add save + cancel button
    button_save = create_my_button(" Save", translated_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    body.append(urwid.Divider())

    body.append(urwid.Text('If you use also raw disks (Atari native ', align='center'))
    body.append(urwid.Text('disks), you should avoid using few      ', align='center'))
    body.append(urwid.Text('letters from C: to leave some space for ', align='center'))
    body.append(urwid.Text('them.                                   ', align='center'))

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)
    shared.main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def on_edit_changed(button, state, data):
    """ edit line changed """
    key = data['id']                        # get key
    shared.settings_changed[key] = state           # store value
    app_log.debug(f"on_edit_changed: {key} -> {state}")


def get_drive_letter(setting_name):
    letter = setting_get_merged(setting_name).upper()

    if len(letter) > 1:
        letter = letter[0]

    letter_int = ord(letter)
    return letter_int


def translated_save(button):
    app_log.debug(f"translated_save: {shared.settings_changed}")

    # translated settings verification before saving here
    first_letter = get_drive_letter('DRIVELETTER_FIRST')
    config_letter = get_drive_letter('DRIVELETTER_CONFDRIVE')
    shared_letter = get_drive_letter('DRIVELETTER_SHARED')
    zip_letter = get_drive_letter('DRIVELETTER_ZIP')

    letters_list = [first_letter, config_letter, shared_letter, zip_letter]
    letters_set = set(letters_list)

    for letter in letters_list:
        if letter < 67 or letter > 80:      # one of the letters is less than 'C' or grater than 'P', fail
            dialog(shared.main_loop, shared.current_body, "Only drive letters from C to P are allowed.")
            return

    if len(letters_set) != len(letters_list):
        dialog(shared.main_loop, shared.current_body, "The specified drive letters must be different.")
        return

    settings_save()
    back_to_main_menu(None)
