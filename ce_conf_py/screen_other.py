import urwid
from loguru import logger as app_log
from urwid_helpers import create_my_button, create_header_footer, dialog
from utils import on_cancel, back_to_main_menu, setting_get_bool, setting_get_str, settings_save, settings_load
import shared
from screen_network import create_setting_row
from screen_translated import create_radio_button_options_rows
from IPy import IP


def other_create(button):
    settings_load()

    header, footer = create_header_footer('Other settings')

    body = []
    body.append(urwid.Divider())

    col1w = 18
    col2w = 17
    col_narrow = 5

    cols = create_setting_row('Update time from internet', 'title', '', 35, 1)
    body.append(cols)

    cols = create_setting_row('Enable', 'checkbox', 1, col1w, col2w, False, 'TIME_SET')
    body.append(cols)

    cols = create_setting_row('NTP server', 'edit', '', col1w, col2w, False, 'TIME_NTP_SERVER')
    body.append(cols)

    cols = create_setting_row('UTC offset', 'edit', '', col1w, col_narrow, False, 'TIME_UTC_OFFSET')
    body.append(cols)

    body.append(urwid.Divider())
    body.append(urwid.Divider())

    cols = create_setting_row('Screencast Frameskip (10-255)', 'text', '', 33, 1)
    body.append(cols)
    cols = create_setting_row('', 'edit', '', col1w, col_narrow, False, 'SCREENCAST_FRAMESKIP')
    body.append(cols)
    body.append(urwid.Divider())

    cols = create_setting_row('Screen res in DESKTOP.INF', 'text', '', 33, 1)
    body.append(cols)

    cols = create_radio_button_options_rows(
        col1w, " ",
        [{'value': 1, 'text': 'low'},
         {'value': 2, 'text': 'mid'}],
        "SCREEN_RESOLUTION")
    body.extend(cols)
    body.append(urwid.Divider())

    # add save + cancel button
    button_save = create_my_button(" Save", other_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)
    shared.main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def validate_setting(setting_name, setting_name_readable, validation_class, value_min, value_max):
    value = 0
    good = False

    try:
        value_str = setting_get_str(setting_name)           # fetch value

        if hasattr(value_str, 'strip'):                     # strip if can (if this is int, won't strip)
            value_str = value_str.strip()                   # remove trailing and leading spaces

        shared.settings_changed[setting_name] = value_str   # store back
        value = validation_class(value_str)                 # let validation_class try to read the value
        good = True
    except Exception as exc:
        app_log.warning(f"failed to convert {value_str} to {validation_class}: {str(exc)}")

    if not good:        # if validation using validation class failed
        app_log.warning(f"validate_setting {setting_name} failed on value {value_str}")
        dialog(shared.main_loop, shared.current_body, f"The {setting_name_readable} '{value_str}' seems to be invalid!")
        return False

    if value_min is not None and value < value_min:         # if should validate using value_min
        app_log.warning(f"validate_setting {setting_name} failed on value {value_str}")
        dialog(shared.main_loop, shared.current_body, f"The {setting_name_readable} value {value} is smaller than {value_min}!")
        return False

    if value_max is not None and value > value_max:         # if should validate using value_max
        app_log.warning(f"validate_setting {setting_name} failed on value {value_str}")
        dialog(shared.main_loop, shared.current_body, f"The {setting_name_readable} value {value} is greater than {value_max}!")
        return False

    # if got here, everything ok
    app_log.debug(f"validate_setting {setting_name} - value {value_str} is OK")
    return True


def other_save(button):
    app_log.debug(f"other_save: {shared.settings_changed}")

    # validate these values always
    checks = [
        ('SCREENCAST_FRAMESKIP', 'frame skip', int, 10, 255),
        ('SCREEN_RESOLUTION', 'screen resolution', int, 1, 2)
    ]

    # validate time settings only if setting time from internet enabled
    if setting_get_bool('TIME_SET'):
        checks.extend([
            ('TIME_NTP_SERVER', 'NTP address', str, None, None),
            ('TIME_UTC_OFFSET', 'UTC offset', float, -12.0, 14.0)
        ])

    # validate values based on check list
    for check in checks:                    # go through all the checks
        good = validate_setting(*check)     # validate one check

        if not good:                        # check failed, quit now
            return

    settings_save()
    back_to_main_menu(None)
