import time
import re
import os
import logging
import urwid
import urllib3
import socket
import threading
from setproctitle import setproctitle
from downloader import DownloaderView
from image_slots import on_show_image_slots
import shared
from shared import load_dotenv_config, get_list_of_lists
from urwid_helpers import create_my_button, create_header_footer

load_dotenv_config()                # load dotenv
app_log = logging.getLogger('root')

thr_download_lists = None


def update_status_alarm(loop=None, data=None):
    """ this gets called on alarm to update the status from main thread, not from download thread """
    shared.last_status_string = shared.new_status_string      # this new string is the last one used

    if shared.text_status:                      # got this widget? set new text
        shared.text_status.set_text(shared.new_status_string)

    shared.main_loop.draw_screen()              # force redraw


def update_status(new_status):
    """ call this method to update status bar on screen """
    app_log.debug(f"status: {new_status}")
    shared.new_status_string = (new_status[:58] + '..') if len(new_status) > 60 else new_status       # cut to 60 chars

    if shared.new_status_string != shared.last_status_string:     # if status string has changed since last set
        if shared.main_loop:    # if got main loop, trigger alarm to redraw widgets to do the update from main thread
            shared.main_loop.set_alarm_in(0.1, update_status_alarm)


def on_show_selected_list(button, choice):
    shared.view_object = DownloaderView()
    shared.view_object.on_show_selected_list(button, choice)


def create_main_menu():
    body = []
    header, footer = create_header_footer('>>> CosmosEx Floppy Tool <<<')

    shared.on_unhandled_keys_handler = None     # no unhandled keys handler on main menu

    for i in range(3):
        body.append(urwid.Divider())

    button = create_my_button('Image slots', on_show_image_slots)
    body.append(button)
    body.append(urwid.Divider())
    body.append(urwid.Divider())

    body.append(urwid.Text('Games to download:'))
    body.append(urwid.Divider())

    lol = get_list_of_lists()
    for index, item in enumerate(lol):           # go through the list of lists, extract names, put them in the buttons
        button = create_my_button(item['name'], on_show_selected_list, item)
        body.append(button)

    if not lol:         # no lists of images?
        body.append(urwid.Text('(no list of images present)'))

    body.append(urwid.Divider())

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)
    return urwid.Frame(w_body, header=header, footer=footer)


if __name__ == "__main__":
    setproctitle("ce_fdd_py")  # set process title

    for dir in [os.getenv('PATH_TO_LISTS'), os.getenv('DATA_DIR'), os.getenv('LOG_DIR')]:
        os.makedirs(dir, exist_ok=True)

    shared.log_config()

    shared.terminal_cols, terminal_rows = urwid.raw_display.Screen().get_cols_rows()
    shared.items_per_page = terminal_rows - 4

    shared.main = urwid.Padding(create_main_menu())

    top = urwid.Overlay(shared.main, urwid.SolidFill(), align='center', width=('relative', 100), valign='middle',
                        height=('relative', 100), min_width=20, min_height=9)

    shared.current_body = top

    try:
        shared.main_loop = urwid.MainLoop(top, palette=[('reversed', 'standout', '')],
                                          unhandled_input=shared.on_unhandled_keys_generic)

        shared.main_loop.run()
    except KeyboardInterrupt:
        print("Terminated by keyboard...")

    shared.should_run = False
