import os
import urwid
from loguru import logger as app_log
from urwid_helpers import create_my_button, create_header_footer, dialog_yes_no, dialog
from utils import on_cancel, back_to_main_menu
import shared
from screen_network import create_setting_row

# only 1 button - Check / Update / Force
# check  - trigger check on button press
# update - when check says new version found -
# force  - when check doesn't find a new version, but you want to re-flash anyway
#        - will show a dialog 'Do you really want to force update? You already seem to have this version installed.' Y/N

screen_shown = False
button_text_current = ' Check '
status_text = 'unknown'
widget_status = None
widget_button = None


def update_create(button):
    header, footer = create_header_footer('Software & Firmware updates')

    body = []

    body.append(urwid.Divider())
    body.append(urwid.Text('Press the button to check for update and then press it again to install it.', align='left'))

    for i in range(3):
        body.append(urwid.Divider())

    col1w = 24
    col2w = 12

    cols = create_setting_row('Status', 'text', '', col1w, col2w, reverse=True)
    body.append(cols)
    body.append(urwid.Divider())

    global widget_status
    widget_status = urwid.Text(status_text)
    body.append(widget_status)

    for i in range(3):
        body.append(urwid.Divider())

    # add update + cancel buttons
    global widget_button
    button_update, widget_button = create_my_button(button_text_current, on_action, return_widget=True)
    button_cancel = create_my_button(" Cancel ", on_cancel)
    buttons = urwid.GridFlow([button_update, button_cancel], 12, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', col1w + col2w)
    shared.main.original_widget = urwid.Frame(w_body, header=header, footer=footer)

    global screen_shown
    screen_shown = True

    on_refresh_update_screen(None, None)        # refresh update screen now

    shared.main_loop.set_alarm_in(1, on_refresh_update_screen)      # trigger alarm in 1 second
    shared.main_loop.run()


def get_status():
    # if no update files present - 'before_check' state
    if not os.path.exists(os.getenv('FILE_UPDATE_STATUS')):
        return 'before_check', False

    # STATUS file exists, but no UPDATE_PENDING_* files present, but there's UPDATE_STATUS file, 'checking' state
    if not os.path.exists(os.getenv('FILE_UPDATE_PENDING_YES')) and not os.path.exists(os.getenv('FILE_UPDATE_PENDING_NO')):
        return 'checking', False

    # if there's UPDATE_PENDING_* file present, it's 'after_check' state
    should_update = os.path.exists(os.getenv('FILE_UPDATE_PENDING_YES'))
    return 'after_check', should_update


def on_update_status_test(status):
    global status_text

    if status == 'before_check':        # nothing to update before check
        return

    try:
        with open(os.getenv('FILE_UPDATE_STATUS')) as f:            # try to open file and read content
            status_text_new = f.read()

        status_text_new = status_text_new.strip()   # remove leading and trailing white spaces

        if status_text == status_text_new:      # status text not changed, quit
            return

        # set new status text
        global widget_status
        widget_status.set_text(status_text_new)

    except Exception as ex:
        app_log.warning('Failed to read update status file: {}'.format(str(ex)))


def on_update_button_text(status, should_update):
    global button_text_current

    status_to_btn_text = {'before_check': ' Check ', 'checking': ' Wait ',
                          'after_check': ' Update ' if should_update else ' Force '}
    btn_text_new = status_to_btn_text.get(status, ' ??? ')  # status to button text

    if button_text_current == btn_text_new:     # the button text doesn't need a change, so quit
        return

    button_text_current = btn_text_new
    widget_button.set_text(btn_text_new)        # set button text


def on_refresh_update_screen(loop=None, data=None):
    global screen_shown

    if not screen_shown:                        # if this screen is already not shown, just quit
        return

    shared.main_loop.set_alarm_in(1, on_refresh_update_screen)  # trigger alarm in 1 second

    status, should_update = get_status()        # get current update check status

    on_update_status_test(status)                       # update status text
    on_update_button_text(status, should_update)        # update button text


def on_cancel(button):
    global screen_shown
    screen_shown = False

    back_to_main_menu(None)  # return to main menu


def on_action(button):
    status, should_update = get_status()        # get current update check status

    if status == 'before_check':                # didn't check yet? run the check
        os.system('/ce/update/check_for_update.sh > /dev/null 2>&1 &')

    elif status == 'after_check':               # we're after the check
        if should_update:                       # should update device? run the update
            on_update_force(True)               # do the checks and start update via this function to reuse code
            return

        # update not really needed, show question
        dialog_yes_no(shared.main_loop, shared.current_body,
                      f"Your device is up to date.\nDo you want to force update anyway?",
                      on_update_force, title='Force update?')


def on_update_force(should_force):
    """ this function gets called when dialog_yes_no terminates with True/False (yes/no) answer """
    if not should_force:        # user didn't want to force update, quit
        return

    # check if update is not already running
    running = os.system('/ce/update/update_running.sh > /dev/null 2>&1 &')
    app_log.debug("update_running: {}".format(running))

    if running > 0:     # if the update is already running
        dialog(shared.main_loop, shared.current_body,
               "Update is already running. Do not turn off your device!")
        return

    # warn that update will take a while
    dialog(shared.main_loop, shared.current_body,
           "Updating device. This will could take up to 5 minutes. Do not turn off your device!")

    # user selected force update? run it
    app_log.debug("starting update now")
    os.system('/ce/ce_update.sh > /dev/null 2>&1 &')

    on_cancel(None)         # back to main menu
