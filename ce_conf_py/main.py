import os
import urwid
from setproctitle import setproctitle
import logging
from urwid_helpers import create_my_button, create_header_footer
from screen_license import license_create
from screen_ids import acsi_ids_create
from screen_translated import translated_create
from screen_hdd_image import hdd_image_create
from screen_shared import shared_drive_create
from screen_floppy import floppy_create
from screen_network import network_create
from screen_ikbd import ikbd_create
from screen_update import update_create
import shared
from utils import delete_update_files, log_config, other_instance_running, load_dotenv_config

app_log = logging.getLogger()


def alarm_callback(loop=None, data=None):
    """ this gets called on alarm """
    pass


def update_status(new_status):
    """ call this method to update status bar on screen """
    if shared.main_loop:  # if got main loop, trigger alarm to redraw widgets
        shared.main_loop.set_alarm_in(1, alarm_callback)


def create_main_menu():
    body = []
    header, footer = create_header_footer('CE Config - main menu')

    menu_items = [('ACSI IDs', acsi_ids_create),
                  ('Translated disks', translated_create),
                  ('Hard Disk Image', hdd_image_create),
                  ('Shared drive', shared_drive_create),
                  ('Floppy config', floppy_create),
                  ('Network settings', network_create),
                  ('IKBD', ikbd_create),
                  ('Update software', update_create)]

    if False:       # if should show license menu
        menu_items.insert(0, ('! License key !', license_create))
    else:
        body.append(urwid.Divider())

    # create main menu buttons
    for btn_text, handler in menu_items:
        button = create_my_button(btn_text, handler)
        button = urwid.Padding(button, 'center', 20)        # center buttons, exact width 20 chars
        body.append(button)
        body.append(urwid.Divider())

    body.append(urwid.Divider())

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)
    return urwid.Frame(w_body, header=header, footer=footer)


def exit_program(button):
    raise urwid.ExitMainLoop()


# the config tool execution starts here
if __name__ == "__main__":
    setproctitle("ce_config")  # set process title

    load_dotenv_config()
    log_config()

    for one_dir in [os.getenv('LOG_DIR'), os.getenv('SETTINGS_DIR')]:
        os.makedirs(one_dir, exist_ok=True)

    # check if other instance is running, quit if it is
    if other_instance_running():
        print("Other instance is running, this instance won't run!")
        app_log.info("Other instance is running, this instance won't run!")
        exit(1)

    delete_update_files()

    shared.terminal_cols, shared.terminal_rows = urwid.raw_display.Screen().get_cols_rows()
    shared.items_per_page = shared.terminal_rows - 4

    shared.main = urwid.Padding(create_main_menu(), left=2, right=2)

    top = urwid.Overlay(shared.main, urwid.SolidFill(),
                        align='center', width=('relative', 100),
                        valign='middle', height=('relative', 100),
                        min_width=20, min_height=9)

    shared.current_body = top

    try:
        shared.main_loop = urwid.MainLoop(top, palette=[('reversed', 'standout', '')])
        shared.main_loop.run()
    except KeyboardInterrupt:
        print("Terminated by keyboard...")

    should_run = False
