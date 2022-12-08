import urwid
import logging
from urwid_helpers import create_my_button, create_header_footer, MyRadioButton, dialog
from utils import settings_load, settings_save, on_cancel, back_to_main_menu, setting_get_int
import shared

app_log = logging.getLogger()


def acsi_ids_create(button):
    settings_load()

    header, footer = create_header_footer('ACSI IDs config')

    body = []
    body.append(urwid.Divider())

    width = 6
    width_first = width + 2

    # add header row to all IDs config
    cols = urwid.Columns([
        ('fixed', width_first, urwid.Text("ID")),
        ('fixed', width, urwid.Text(" off")),
        ('fixed', width, urwid.Text(" sd")),
        ('fixed', width, urwid.Text(" raw")),
        ('fixed', width, urwid.Text("ce_dd"))],
        dividechars=0)

    body.append(cols)

    # fill in each row
    for id_ in range(8):
        id_str = urwid.Text(f" {id_}")          # ID number

        key = f'ACSI_DEVTYPE_{id_}'             # construct setting name
        selected = setting_get_int(key)
        app_log.debug(f"on_screen_acsi_config: {key} -> {selected}")

        bgroup = []                             # button group
        b1 = MyRadioButton(bgroup, u'', on_state_change=acsi_ids_changed, user_data={'id': id_, 'value': 0}, state=(selected == 0))  # off
        b2 = MyRadioButton(bgroup, u'', on_state_change=acsi_ids_changed, user_data={'id': id_, 'value': 1}, state=(selected == 1))  # sd
        b3 = MyRadioButton(bgroup, u'', on_state_change=acsi_ids_changed, user_data={'id': id_, 'value': 2}, state=(selected == 2))  # raw
        b4 = MyRadioButton(bgroup, u'', on_state_change=acsi_ids_changed, user_data={'id': id_, 'value': 3}, state=(selected == 3))  # ce_dd

        # put items into Columns (== in a Row)
        cols = urwid.Columns([
            ('fixed', width_first, id_str),
            ('fixed', width, b1),
            ('fixed', width, b2),
            ('fixed', width, b3),
            ('fixed', width, b4)],
            dividechars=0)

        body.append(cols)

    body.append(urwid.Divider())

    # add save + cancel button
    button_save = create_my_button(" Save", acsi_ids_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    body.append(urwid.Divider())
    body.append(urwid.Divider())

    body.append(urwid.Text('off   - turned off, not responding here ', align='center'))
    body.append(urwid.Text('sd    - sd card (only one)              ', align='center'))
    body.append(urwid.Text('raw   - raw sector access (use HDDr/ICD)', align='center'))
    body.append(urwid.Text('ce_dd - for booting CE_DD driver        ', align='center'))

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)
    shared.main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def acsi_ids_changed(button, state, data):
    """ when ACSI ID setting changes from one value to another (e.g. off -> sd) """
    if not state:                   # called on the checkbox, which is now off? skip it
        return

    id_ = data['id']                        # get ID
    key = f'ACSI_DEVTYPE_{id_}'             # construct key name
    value = data['value']
    shared.settings_changed[key] = value           # store value
    app_log.debug(f"on_acsi_id_changed: {key} -> {value}")


def acsi_ids_save(button):
    """ function that gets called on saving ACSI IDs config """

    global settings

    something_active = False
    count_sd = 0
    count_translated = 0

    id_types = {}
    for id_ in range(8):
        key = f'ACSI_DEVTYPE_{id_}'
        id_types[key] = int(shared.settings.get(key, 0))   # get settings before change

        new_type = shared.settings_changed.get(key)        # try to get the changed value

        if new_type is not None:                    # if value was found, store it
            id_types[key] = new_type

        if id_types[key] != 0:                      # if found something that is not OFF, got something active
            something_active = True

        if id_types[key] == 1:                      # found SD card type
            count_sd += 1

        if id_types[key] == 3:                      # found translated type
            count_translated += 1

    app_log.debug(f"on acsi save: {id_types}")

    # after the loop validate settings, show warning
    if not something_active:
        dialog(shared.main_loop, shared.current_body,
               "All ACSI/SCSI IDs are set to 'OFF', this is invalid and would brick the device. "
               "Select at least one active ACSI/SCSI ID.")
        return

    if count_translated > 1:
        dialog(shared.main_loop, shared.current_body, f"You have {count_translated} IDs selected as CE_DD type. Unselect some to leave only 1 active.")
        return

    if count_sd > 1:
        dialog(shared.main_loop, shared.current_body, f"You have {count_sd} IDs selected as SD type. Unselect some to leave only 1 active.")
        return

    # if(hwConfig.hddIface == HDD_IF_SCSI) {                         // running on SCSI? Show warning if ID 0 or 7 is used
    #if (devTypes[0] != DEVTYPE_OFF | | devTypes[7] != DEVTYPE_OFF) {
    #showMessageScreen("Warning", "You assigned something to ID 0 or ID 7.\n\rThey might not work as they might be\n\rused by SCSI controller.\n\r");
    #}

    settings_save()

    back_to_main_menu(button)
