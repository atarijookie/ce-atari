import os
import queue
import math
import codecs
import urwid
import subprocess
import logging
import shared
from urwid_helpers import create_my_button, dialog, back_to_main_menu, create_header_footer

app_log = logging.getLogger()

widget_image_name = {}
txt_image_name = {0: 'DBUG_003.MSA'}        # fetch current image names here

widget_image_content = {}
txt_image_content = {}                      # fetch content by image name here


def on_show_image_slots(button):
    header, footer = create_header_footer('Floppy Image Slots')

    body = []
    body.append(urwid.Divider())

    # TODO: get current images and their content

    global txt_image_name, widget_image_name, txt_image_content, widget_image_content

    for i in range(3):
        # create buttons and texts for image # i
        btn_insert = create_my_button("Insert", on_insert, i)
        btn_eject = create_my_button("Eject", on_eject, i)

        txt_name = txt_image_name.get(i, None)
        txt_name = txt_name if txt_name else '(  empty  )'
        widget_image_name[i] = urwid.Text(txt_name)

        cols = urwid.Columns([
            ('fixed', 6, urwid.Text(f'Slot {i + 1}')),
            ('fixed', 12, widget_image_name[i]),
            ('fixed', 10, btn_insert),
            ('fixed', 9, btn_eject)
            ], dividechars=1)

        body.append(cols)

        # create text widget for image # i content
        widget_image_content[i] = urwid.Text(txt_image_content.get(i, ''))
        body.append(widget_image_content[i])

        for n in range(2):
            body.append(urwid.Divider())

    # now just main menu button and we're done
    button_back = create_my_button("Main menu", back_to_main_menu)
    buttons = urwid.GridFlow([button_back], 13, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 40)
    shared.main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def on_insert(button, index):
    pass


def on_eject(button, index):
    pass

