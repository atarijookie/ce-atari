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


class DialogExit(Exception):
    pass


# dialogs taken from:
# https://github.com/urwid/urwid/blob/master/examples/dialog.py

class DialogDisplay:
    palette = [
        ('body','black','light gray', 'standout'),
        ('border','black','dark blue'),
        ('shadow','white','black'),
        ('selectable','black', 'dark cyan'),
        ('focus','white','dark blue','bold'),
        ('focustext','light gray','dark blue'),
        ]

    def __init__(self, text, height, width, body=None):
        width = int(width)
        if width <= 0:
            width = ('relative', 80)
        height = int(height)
        if height <= 0:
            height = ('relative', 80)

        self.body = body
        if body is None:
            # fill space with nothing
            body = urwid.Filler(urwid.Divider(),'top')

        self.frame = urwid.Frame( body, focus_part='footer')
        if text is not None:
            self.frame.header = urwid.Pile( [urwid.Text(text),
                urwid.Divider()] )
        w = self.frame

        # pad area around listbox
        w = urwid.Padding(w, ('fixed left',2), ('fixed right',2))
        w = urwid.Filler(w, ('fixed top',1), ('fixed bottom',1))
        w = urwid.AttrWrap(w, 'body')

        # "shadow" effect
        w = urwid.Columns( [w,('fixed', 2, urwid.AttrWrap(
            urwid.Filler(urwid.Text(('border','  ')), "top")
            ,'shadow'))])
        w = urwid.Frame( w, footer =
            urwid.AttrWrap(urwid.Text(('border','  ')),'shadow'))

        # outermost border area
        w = urwid.Padding(w, 'center', width )
        w = urwid.Filler(w, 'middle', height )
        w = urwid.AttrWrap( w, 'border' )

        self.view = w


    def add_buttons(self, buttons):
        l = []
        for name, exitcode in buttons:
            b = urwid.Button( name, self.button_press )
            b.exitcode = exitcode
            b = urwid.AttrWrap( b, 'selectable','focus' )
            l.append( b )
        self.buttons = urwid.GridFlow(l, 10, 3, 1, 'center')
        self.frame.footer = urwid.Pile( [ urwid.Divider(),
            self.buttons ], focus_item = 1)

    def button_press(self, button):
        raise DialogExit(button.exitcode)

    def main(self):
        self.loop = urwid.MainLoop(self.view, self.palette)
        try:
            self.loop.run()
        except DialogExit as e:
            return self.on_exit( e.args[0] )

    def on_exit(self, exitcode):
        return exitcode, ""


class TextDialogDisplay(DialogDisplay):
    def __init__(self, message, height, width):
        l = []
        l.append(urwid.Text(message))

        body = urwid.ListBox(urwid.SimpleListWalker(l))
        body = urwid.AttrWrap(body, 'selectable', 'focustext')

        DialogDisplay.__init__(self, None, height, width, body)

    def unhandled_key(self, size, k):
        if k in ('up','page up','down','page down'):
            self.frame.set_focus('body')
            self.view.keypress( size, k )
            self.frame.set_focus('footer')


def show_text_dialog(message, height=5, width=30):
    d = TextDialogDisplay(message, height, width)
    d.add_buttons([("Exit", 0)])
    d.main()

