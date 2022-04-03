import urwid
import logging
from urwid_helpers import create_my_button, create_header_footer, create_edit, MyCheckBox, dialog
from utils import settings_load, settings_save, on_cancel, back_to_main_menu, setting_get_bool, on_checkbox_changed, \
    setting_get_str, on_editline_changed
import shared

app_log = logging.getLogger()


def ikbd_checkbox_line(label, hint, setting_name):
    body = []

    checked = setting_get_bool(setting_name)        # get if should be checked

    checkbox = MyCheckBox('', state=checked, on_state_change=on_ikbd_checkbox_changed)
    checkbox.setting_name = setting_name

    # first line with label and checkbox
    cols = urwid.Columns([
        ('fixed', 31, urwid.Text(label)),
        ('fixed', 5, checkbox)],
        dividechars=0)
    body.append(cols)

    # second line with hint
    if hint:
        body.append(urwid.Text(hint))

    # third line with divider
    body.append(urwid.Divider())

    return body


def get_key_setting_names(joy_no):
    """ construct temporary joy settings names for joystick, e.g.:
    JOY0_LEFT, JOY0_DOWN, JOY0_RIGHT, JOY0_UP, JOY0_BUTTON
    """

    setting_names = []

    joy_name = 'JOY0' if joy_no == 0 else 'JOY1'

    for key_name in ['LEFT', 'DOWN', 'RIGHT', 'UP', 'BUTTON']:
        key_name_full = f'{joy_name}_{key_name}'
        setting_names.append(key_name_full)                     # store key names to list

    return setting_names


def ikbd_keyboard_joystick(joy_no):
    body = []

    setting_name = 'KEYBOARD_KEYS_JOY0' if joy_no == 0 else 'KEYBOARD_KEYS_JOY1'        # get correct setting name
    keys = setting_get_str(setting_name)        # get keys from settings

    if not keys:                # keys seem to be empty? use some defaults
        keys = "A%S%D%W%LSHIFT" if joy_no == 0 else "LEFT%DOWN%RIGHT%UP%RSHIFT"

    keys = keys.split('%')      # string to list

    if len(keys) != 5:          # wrong length? replace with defaults
        keys = ['A', 'S', 'D', 'W', 'LSHIFT'] if joy_no == 0 else ['LEFT', 'DOWN', 'RIGHT', 'UP', 'RSHIFT']

    # construct temporary joy settings names
    setting_names = get_key_setting_names(joy_no)

    for i, key_name in enumerate(setting_names):        # store current values under temporary setting names
        shared.settings_changed[key_name] = keys[i]

    w_edit = 12
    cols_btn, _ = create_edit(setting_names[4], w_edit, on_editline_changed)
    cols_up, _ = create_edit(setting_names[3], w_edit, on_editline_changed)
    cols_left, _ = create_edit(setting_names[0], w_edit, on_editline_changed)
    cols_down, _ = create_edit(setting_names[1], w_edit, on_editline_changed)
    cols_right, _ = create_edit(setting_names[2], w_edit, on_editline_changed)

    # button key and up key
    cols = urwid.Columns([
        ('fixed', w_edit, cols_btn),
        ('fixed', w_edit, cols_up),
        ('fixed', w_edit, urwid.Text(''))],
        dividechars=0)
    body.append(cols)

    # button left, down, right
    cols = urwid.Columns([
        ('fixed', w_edit, cols_left),
        ('fixed', w_edit, cols_down),
        ('fixed', w_edit, cols_right)],
        dividechars=0)
    body.append(cols)

    # divider
    body.append(urwid.Divider())

    return body


def ikbd_create(button):
    settings_load()

    header, footer = create_header_footer('IKBD settings')

    body = []

    # attach 1st joy as JOY 0
    chb_line = ikbd_checkbox_line('Attach 1st joy as JOY 0', '(hotkey: CTRL+any SHIFT+HELP/F11)', 'JOY_FIRST_IS_0')
    body.extend(chb_line)

    # mouse wheel as arrow up / down
    chb_line = ikbd_checkbox_line('Mouse wheel as arrow UP / DOWN', '', 'MOUSE_WHEEL_AS_KEYS')
    body.extend(chb_line)

    # Keyboard Joy 0 enabled
    body.append(urwid.Padding(urwid.AttrMap(urwid.Text(''), 'reversed'), 'center', 40))     # inverse divider line
    chb_line = ikbd_checkbox_line('Keyboard Joy 0 enabled', '(hotkey: CTRL+LSHIFT+UNDO/F12)', 'KEYBORD_JOY0')
    body.extend(chb_line)

    # keyboard buttons for joy 0
    cols_joy0 = ikbd_keyboard_joystick(0)
    body.extend(cols_joy0)

    # Keyboard Joy 1 enabled
    body.append(urwid.Padding(urwid.AttrMap(urwid.Text(''), 'reversed'), 'center', 40))     # inverse divider line
    chb_line = ikbd_checkbox_line('Keyboard Joy 1 enabled', '(hotkey: CTRL+RSHIFT+UNDO/F12)', 'KEYBORD_JOY1')
    body.extend(chb_line)

    # keyboard buttons for joy 1
    cols_joy1 = ikbd_keyboard_joystick(1)
    body.extend(cols_joy1)

    # add save + cancel button
    button_save = create_my_button(" Save", ikbd_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 36)
    shared.main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def on_ikbd_checkbox_changed(checkbox, state):
    setting_name = checkbox.setting_name
    on_checkbox_changed(setting_name, state)


def validate_joy_get_values(joy_no):
    setting_names = get_key_setting_names(joy_no)

    values = []
    for name in setting_names:          # go through all the settings
        value = setting_get_str(name)   # get value

        if not value:                   # value empty?
            dialog(shared.main_loop, shared.current_body, f"Keyboard Joy {joy_no} has empty key!")
            return False, ""

        value = value.upper()           # to uppercase

        if len(value) > 1:              # not a single key? verify if it's valid key name
            if value not in ['LEFT', 'DOWN', 'RIGHT', 'UP', 'RSHIFT', 'LSHIFT', 'RCTRL', 'LCTRL', 'LALT', 'RALT',
                             'TAB', 'ENTER', 'BACKSPACE', 'CAPS', 'HOME', 'INSERT', 'DELETE', 'END']:
                dialog(shared.main_loop, shared.current_body, f"Keyboard Joy {joy_no} has invalid value: {value}")
                return False, ""

        values.append(value)            # append to list of keys

    values_set = set(values)            # remove duplicates by converting to set

    # some key is now missing? some key has been used more than once
    if len(values) != len(values_set):
        dialog(shared.main_loop, shared.current_body, f"Keyboard Joy {joy_no} has some key used more than once!")
        return False, ""

    values = "%".join(values)
    return True, values


def remove_temp_joy_settings(joy_no):
    names = get_key_setting_names(joy_no)       # get setting names

    for name in names:          # remove all these names from changed settings
        shared.settings_changed.pop(name, None)


def ikbd_save(button):
    app_log.debug(f"ikbd_save: {shared.settings_changed}")

    good, values0 = validate_joy_get_values(0)
    if not good:        # some invalid value in joy keys? quit
        return

    good, values1 = validate_joy_get_values(1)
    if not good:        # some invalid value in joy keys? quit
        return

    # store the merged keyboard joystick keys
    shared.settings_changed['KEYBOARD_KEYS_JOY0'] = values0
    shared.settings_changed['KEYBOARD_KEYS_JOY1'] = values1

    # remove the temporary joystick keys
    remove_temp_joy_settings(0)
    remove_temp_joy_settings(1)

    # save and return
    settings_save()
    back_to_main_menu(None)
