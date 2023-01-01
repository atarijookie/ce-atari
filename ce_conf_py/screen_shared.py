import urwid
from loguru import logger as app_log
from IPy import IP
from urwid_helpers import create_my_button, create_header_footer, create_edit, MyCheckBox, dialog, \
    create_radio_button_options_rows
from utils import settings_load, settings_save, on_cancel, back_to_main_menu, setting_get_bool, setting_get_str, \
    on_checkbox_changed, on_editline_changed
import shared


def shared_drive_create(button):
    settings_load()

    header, footer = create_header_footer('Shared drive settings')

    body = []
    body.append(urwid.Text('Define what folder on which machine will', align='center'))
    body.append(urwid.Text('be used as drive mounted through network', align='center'))
    body.append(urwid.Text('on CosmosEx. Works in translated mode.  ', align='center'))
    body.append(urwid.Divider())

    btn_enabled = MyCheckBox('', on_state_change=on_enabled_changed)
    btn_enabled.set_state(setting_get_bool('SHARED_ENABLED'))

    # enabling / disabling shared drive
    cols = urwid.Columns([
        ('fixed', 10, urwid.Text('Enabled', align='left')),
        ('fixed', 7, btn_enabled)],
        dividechars=0)
    body.append(cols)

    # shared drive protocol (NFS or samba)
    body.append(urwid.Divider())

    cols = create_radio_button_options_rows(
        10, "Protocol",
        [{'value': 0, 'text': 'Samba / cifs / windows'},
         {'value': 1, 'text': 'NFS'}],
        "SHARED_NFS_NOT_SAMBA")
    body.extend(cols)
    body.append(urwid.Divider())

    # IP of machine sharing info
    cols_edit_ip, _ = create_edit('SHARED_ADDRESS', 17, on_editline_changed)

    cols = urwid.Columns([              # NFS option row
        ('fixed', 10, urwid.Text('IP addr.', align='left')),
        ('fixed', 17, cols_edit_ip)],
        dividechars=0)
    body.append(cols)
    body.append(urwid.Divider())

    # folder on sharing machine
    body.append(urwid.Text('Shared folder path on server', align='left'))

    cols_edit_path, _ = create_edit('SHARED_PATH', 40, on_editline_changed)
    body.append(cols_edit_path)
    body.append(urwid.Divider())

    # username and password
    cols_edit_username, _ = create_edit('SHARED_USERNAME', 20, on_editline_changed)
    cols = urwid.Columns([
        ('fixed', 10, urwid.Text('Username', align='left')),
        ('fixed', 20, cols_edit_username)],
        dividechars=0)
    body.append(cols)

    cols_edit_password, _ = create_edit('SHARED_PASSWORD', 20, on_editline_changed, mask='*')
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


def on_enabled_changed(button, state):
    on_checkbox_changed('SHARED_ENABLED', state)


def shared_drive_save(button):
    app_log.debug(f"shared_drive_save: {shared.settings_changed}")

    enabled = setting_get_bool('SHARED_ENABLED')

    if enabled:     # verify IP if shared drive is enabled
        good = False

        try:
            ip_str = setting_get_str('SHARED_ADDRESS')
            IP(ip_str)      # let IPy try to read the addr
            good = True
        except Exception as exc:
            app_log.warning(f"failed to convert {ip_str} to IP: {str(exc)}")

        if not good:
            dialog(shared.main_loop, shared.current_body, f"The IP address {ip_str} seems to be invalid!")
            return

        path_str = setting_get_str('SHARED_PATH')

        if not path_str:
            dialog(shared.main_loop, shared.current_body, f"The share path cannot be empty!")
            return

    settings_save()
    back_to_main_menu(None)
