import urwid
import shared
from loguru import logger as app_log


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

    def set_text(self, text):
        self._label.set_text(text)


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
        return (67 <= och <= 90) or (99 <= och <= 122)      # C-Z

    def keypress(self, size, key):
        if self.valid_char(key):        # valid key, use it
            new_text = str(key).upper()
            self.set_text(new_text)
            self._emit("postchange", new_text)
        else:                           # key wasn't handled
            return key


def create_edit_one(setting_name, on_edit_changed):
    letter = shared.settings.get(setting_name, 'C')

    edit_one = EditOne(letter)
    urwid.connect_signal(edit_one, 'postchange', on_edit_changed, {'id': setting_name})
    edit_one_decorated = urwid.AttrMap(edit_one, None, focus_map='reversed')

    cols = urwid.Columns([
        ('fixed', 2, urwid.Text('[ ')),
        ('fixed', 1, edit_one_decorated),
        ('fixed', 2, urwid.Text(' ]'))],
        dividechars=0)

    return cols


def create_my_button(text, on_clicked_fn, on_clicked_data=None, return_widget=False):
    button = MyButton(text, on_clicked_fn, on_clicked_data)
    attrmap = urwid.AttrMap(button, None, focus_map='reversed')  # reversed on button focused
    attrmap.user_data = on_clicked_data

    if return_widget:           # if should also return button widget itself
        return attrmap, button

    return attrmap


def create_header_footer(header_text, footer_text=None):
    header = urwid.AttrMap(urwid.Text(header_text, align='center'), 'reversed')
    header = urwid.Padding(header, 'center', 40)

    if footer_text is None:
        footer_text = 'Ctrl+C or F10 - quit'

    footer = urwid.AttrMap(urwid.Text(footer_text, align='center'), 'reversed')
    footer = urwid.Padding(footer, 'center', 40)

    return header, footer


def create_edit(setting_name, width, on_edit_changed, mask=None):
    from utils import setting_get_str

    if setting_name:        # setting name provided? load setting value
        text = setting_get_str(setting_name)
    else:                   # no setting name? just empty string
        text = ''

    if not isinstance(text, str):       # convert to text if not a text already
        text = str(text)

    app_log.debug(f"create_edit - setting_name: {setting_name}, text: {text}")

    edit_line = urwid.Edit(caption='', edit_text=text, mask=mask)
    urwid.connect_signal(edit_line, 'change', on_edit_changed, {'id': setting_name})

    edit_decorated = urwid.AttrMap(edit_line, None, focus_map='reversed')

    cols = urwid.Columns([
        ('fixed', 1, urwid.Text('[')),
        ('fixed', width - 2, edit_decorated),
        ('fixed', 1, urwid.Text(']'))],
        dividechars=0)

    return cols, edit_line


main_loop_original = None
current_body_original = None
fun_call_on_answer = None


def dialog(main_loop, current_body, text, call_on_answer=None, title='Warning'):
    dialog_yes_no_ok(main_loop, current_body, text, call_on_answer, True, title)


def dialog_yes_no(main_loop, current_body, text, call_on_answer, title='Question'):
    dialog_yes_no_ok(main_loop, current_body, text, call_on_answer, False, title)


def dialog_yes_no_ok(main_loop, current_body, text, call_on_answer, is_ok_dialog, title='Title'):
    """
    Overlays a dialog box on top of the console UI

    Args:
        main_loop: original main loop
        current_body: current body shown on screen
        text: message to display
        call_on_answer: dialog will call this function when answer from user is selected
        title: dialog title message
        is_ok_dialog: if True, shows 'OK' button, if False then shows Yes+No buttons
    """

    # store the original values, so we can restore then on dialog end
    global main_loop_original, current_body_original, fun_call_on_answer
    main_loop_original = main_loop
    current_body_original = current_body
    fun_call_on_answer = call_on_answer

    # header
    header_text = urwid.AttrMap(urwid.Text(title, align='center'), 'reversed')
    header = urwid.AttrMap(header_text, 'banner')

    # Body
    body_text = urwid.Text(text, align='center')
    body_filler = urwid.Filler(body_text)
    body_padding = urwid.Padding(body_filler, left=1, right=1)

    # Footer
    if is_ok_dialog:        # dialog with OK button?
        footer = create_my_button(' OK', on_btn_yes)
        footer = urwid.AttrWrap(footer, 'selectable', 'focus')
        footer = urwid.GridFlow([footer], 8, 1, 1, 'center')
    else:                   # dialog with Yes/No buttons?
        btn_yes = create_my_button('Yes', on_btn_yes)
        btn_no = create_my_button(' No', on_btn_no)
        footer = urwid.GridFlow([btn_yes, btn_no], 8, 1, 1, 'center')

    # Layout
    layout = urwid.Frame(body_padding, header=header, footer=footer, focus_part='footer')

    w = urwid.Overlay(urwid.LineBox(layout, tlcorner='+', tline='-', lline='|', trcorner='+', blcorner='+', rline='|',
                      bline='-', brcorner='+'), current_body, align='center', width=36, valign='middle', height=10)
    main_loop.widget = w


def on_btn_yes(button):                 # called when YES button is pressed
    on_button_yes_no(button, True)


def on_btn_no(button):                  # called when NO button is pressed
    on_button_yes_no(button, False)


def on_button_yes_no(button, yes_pressed):
    reset_layout(button)                # reset layout

    global fun_call_on_answer
    if fun_call_on_answer:                  # if callback function was provided
        fun_call_on_answer(yes_pressed)     # call the callback


def reset_layout(button):
    """ resets the console UI to the previous layout """
    main_loop_original.widget = current_body_original
    main_loop_original.draw_screen()


def create_radio_button_options_rows(label_width, label, options_list, setting_name, total_width=40):
    """ read setting with setting_name from settings, then create few radio buttons based on
    options_list, where each options_list should have 'value' and 'text' """

    is_bool = isinstance(options_list[0]['value'], bool)      # are provided values provided bools?

    from utils import setting_get_bool, setting_get_int, on_option_changed
    value = setting_get_bool(setting_name) if is_bool else setting_get_int(setting_name)

    bgroup = []  # button group
    cols = []

    for idx, option in enumerate(options_list):
        mrb = MyRadioButton(
            bgroup, '', on_state_change=on_option_changed,
            user_data={'id': setting_name, 'value': option['value']})

        if option['value'] == value:        # if the value of this option matches the one from settings, it's selected
            mrb.set_state(True)

        width_rest = total_width - 6 - label_width
        text_for_row = label if idx == 0 else ''
        col = urwid.Columns([
            ('fixed', label_width, urwid.Text(text_for_row)),
            ('fixed', 6, mrb),
            ('fixed', width_rest, urwid.Text(option['text']))],
            dividechars=0)
        cols.append(col)

    return cols
