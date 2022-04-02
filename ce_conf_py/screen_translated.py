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

        edit_one = create_edit_one(setting_name, on_edit_changed)

        cols = urwid.Columns([
            ('fixed', col1, urwid.Text(label)),
            ('fixed', col2, edit_one)],
            dividechars=0)

        return cols

    # translated drive letter
    trans_first = create_drive_row("First translated drive", 'DRIVELETTER_FIRST')
    body.append(trans_first)
    body.append(urwid.Divider())

    # shared drive letter
    trans_shared = create_drive_row("Shared drive", 'DRIVELETTER_SHARED')
    body.append(trans_shared)

    # config drive letter
    trans_config = create_drive_row("Config drive", 'DRIVELETTER_CONFDRIVE')
    body.append(trans_config)

    body.append(urwid.Divider())
    body.append(urwid.AttrMap(urwid.Text('Options', align='center'), 'reversed'))
    body.append(urwid.Divider())

    def create_options_rows(label, option1_true, option2_false, setting_name):
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
            ('fixed', 6, b1),
            ('fixed', 10, urwid.Text(option1_true))],
            dividechars=0)

        cols2_ = urwid.Columns([
            ('fixed', 21, urwid.Text('')),
            ('fixed', 6, b2),
            ('fixed', 10, urwid.Text(option2_false))],
            dividechars=0)

        return cols1_, cols2_

    cols1, cols2 = create_options_rows("Mount USB media as", "raw", "translated", 'MOUNT_RAW_NOT_TRANS')
    body.extend([cols1, cols2, urwid.Divider()])

    cols1, cols2 = create_options_rows("Access ZIP files as", "dirs", "files", 'MOUNT_ZIP_FILES')
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


def translated_save(button):
    app_log.debug(f"translated_save: {shared.settings_changed}")

    # translated settings verification before saving here
    a = setting_get_merged('DRIVELETTER_FIRST')
    b = setting_get_merged('DRIVELETTER_SHARED')
    c = setting_get_merged('DRIVELETTER_CONFDRIVE')

    # if some shared letters is the same, warn and don't save
    if a == b or a == c or b == c:
        dialog(shared.main_loop, shared.current_body, "You must specify different drive letters!")
        return

    settings_save()
    back_to_main_menu(None)

