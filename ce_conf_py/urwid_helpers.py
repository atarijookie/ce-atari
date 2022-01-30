import urwid


class ButtonLabel(urwid.SelectableIcon):
    """ to hide curson on button, move cursor position outside of button
    """

    def set_text(self, label):
        self.__super.set_text(label)
        self._cursor_position = len(label) + 1  # move cursor position outside of button


class MyButton(urwid.Button):
    button_left = "["
    button_right = "]"

    def __init__(self, label, on_press=None, user_data=None):
        self._label = ButtonLabel('')
        self.user_data = user_data

        cols = urwid.Columns([
            ('fixed', len(self.button_left), urwid.Text(self.button_left)),
            self._label,
            ('fixed', len(self.button_right), urwid.Text(self.button_right))],
            dividechars=1)
        super(urwid.Button, self).__init__(cols)

        if on_press:
            urwid.connect_signal(self, 'click', on_press, user_data)

        self.set_label(label)


class MyRadioButton(urwid.RadioButton):
    states = {
        True: urwid.SelectableIcon("[ * ]", 2),
        False: urwid.SelectableIcon("[   ]", 2),
        'mixed': urwid.SelectableIcon("[ # ]", 2)
    }
    reserve_columns = 5


class MyCheckBox(urwid.CheckBox):
    states = {
        True: urwid.SelectableIcon("[ * ]", 2),
        False: urwid.SelectableIcon("[   ]", 2),
        'mixed': urwid.SelectableIcon("[ # ]", 2)
    }
    reserve_columns = 5


class EditOne(urwid.Text):
    _selectable = True
    ignore_focus = False
    # (this variable is picked up by the MetaSignals metaclass)
    signals = ["change", "postchange"]

    def __init__(self, edit_text):
        super().__init__(markup=edit_text)

    def valid_char(self, ch):
        if len(ch) != 1:
            return False

        och = ord(ch)
        return (65 <= och <= 90) or (97 <= och <= 122)

    def keypress(self, size, key):
        if self.valid_char(key):        # valid key, use it
            new_text = str(key).upper()
            self.set_text(new_text)
        else:                           # key wasn't handled
            return key


def create_edit_one(edit_text):
    edit_one = EditOne(edit_text)
    edit_one_decorated = urwid.AttrMap(edit_one, None, focus_map='reversed')

    cols = urwid.Columns([
        ('fixed', 2, urwid.Text('[ ')),
        ('fixed', 1, edit_one_decorated),
        ('fixed', 2, urwid.Text(' ]'))],
        dividechars=0)

    return cols


def create_my_button(text, on_clicked_fn, on_clicked_data=None):
    button = MyButton(text, on_clicked_fn, on_clicked_data)
    attrmap = urwid.AttrMap(button, None, focus_map='reversed')  # reversed on button focused
    attrmap.user_data = on_clicked_data
    return attrmap


def create_header_footer(header_text, footer_text=None):
    header = urwid.AttrMap(urwid.Text(header_text, align='center'), 'reversed')
    header = urwid.Padding(header, 'center', 40)

    if footer_text is None:
        footer_text = 'F5 - refresh, Ctrl+C or F10 - quit'

    footer = urwid.AttrMap(urwid.Text(footer_text, align='center'), 'reversed')
    footer = urwid.Padding(footer, 'center', 40)

    return header, footer


def create_edit(text, width):
    edit_line = urwid.Edit(caption='', edit_text=text)
    edit_decorated = urwid.AttrMap(edit_line, None, focus_map='reversed')

    cols = urwid.Columns([
        ('fixed', 1, urwid.Text('[')),
        ('fixed', width - 2, edit_decorated),
        ('fixed', 1, urwid.Text(']'))],
        dividechars=0)

    return cols


main_loop_original = None
current_body_original = None


def dialog(main_loop, current_body, text = ['']):
    """
    Overlays a dialog box on top of the console UI

    Args:
        test (list): A list of strings to display
    """

    global main_loop_original, current_body_original
    main_loop_original = main_loop
    current_body_original = current_body

    # Header
    header_text = urwid.Text(('banner', 'Help'), align = 'center')
    header = urwid.AttrMap(header_text, 'banner')

    # Body
    body_text = urwid.Text(text, align = 'center')
    body_filler = urwid.Filler(body_text, valign = 'top')
    body_padding = urwid.Padding(
        body_filler,
        left = 1,
        right = 1
    )
    body = urwid.LineBox(body_padding)

    # Footer
    footer = urwid.Button('Okay', reset_layout)
    footer = urwid.AttrWrap(footer, 'selectable', 'focus')
    footer = urwid.GridFlow([footer], 8, 1, 1, 'center')

    # Layout
    layout = urwid.Frame(
        body,
        header = header,
        footer = footer,
        focus_part = 'footer'
    )

    w = urwid.Overlay(
        urwid.LineBox(layout),
        current_body,
        align = 'center',
        width = 40,
        valign = 'middle',
        height = 10
    )

    main_loop.widget = w


def reset_layout(button):
    '''
    Resets the console UI to the default layout
    '''

    main_loop_original.widget = current_body_original
    main_loop_original.draw_screen()
