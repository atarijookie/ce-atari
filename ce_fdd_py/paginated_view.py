import math
import urwid
import logging
import shared
from urwid_helpers import create_my_button, back_to_main_menu, create_header_footer

app_log = logging.getLogger()


class MyDivider(urwid.Divider):
    def keypress(self, tsize, key):
        return key      # key wasn't handled


class PaginatedView:
    title = 'this view title'
    page_current = 1            # start from page 1
    search_phrase = ""          # no search string first
    last_focus_path = None
    main_list_pile = None
    text_pages = None
    list_of_items_filtered = []
    search_phrase_lc = None
    pile_current_page = None
    page_current_zbi = 0
    btn_prev = None
    btn_next = None

    def on_show_selected_list(self, button, choice):
        # load anything you want to show and call show_current_page

        # call this handler on arrows and other unhandled keys
        shared.on_unhandled_keys_handler = self.on_unhandled_keys
        self.show_current_page(None)

    def on_unhandled_keys(self, key):
        """ when some key is not handled, this function is called """

        if self.pile_current_page is None:               # no pile yet? quit
            return None

        if key == 'left':           # left = prev
            self.btn_prev_clicked(None)
            return None

        if key == 'right':          # right = next
            self.btn_next_clicked(None)
            return None

        return key

    def show_current_page(self, button):
        """ show current images page

        This gets called when:
            - first showing list
            - returning back from image submenu
        """
        body = []
        header, footer = create_header_footer(self.title)

        # add search edit line
        widget_search = urwid.Edit("Search: ", edit_text=self.search_phrase)
        urwid.connect_signal(widget_search, 'change', self.search_changed)
        body.append(urwid.AttrMap(widget_search, None, focus_map='reversed'))

        # the next row will hold pages related stuff
        pages_row = []

        btn_prev_decorated, self.btn_prev = create_my_button("prev", self.btn_prev_clicked, return_widget=True)
        pages_row.append(btn_prev_decorated)

        self.text_pages = urwid.Text("0/0", align='center')  # pages showing text
        pages_row.append(self.text_pages)

        btn_next_decorated, self.btn_next = create_my_button("next", self.btn_next_clicked, return_widget=True)
        pages_row.append(btn_next_decorated)

        btn_main = create_my_button("back", self.on_back_button)
        pages_row.append(btn_main)

        body.append(urwid.Columns(pages_row))

        self.update_pile_with_current_buttons()  # first update pile with current buttons
        body.append(self.pile_current_page)  # then add the pile with current page to body

        self.show_page_text()  # show the new page text

        self.main_list_pile = urwid.Pile(body)
        shared.main.original_widget = urwid.Frame(urwid.Filler(self.main_list_pile), header=header, footer=footer)

        if self.last_focus_path is not None:
            self.main_list_pile.set_focus_path(self.last_focus_path)

    def on_back_button(self, button):
        # default back button action - main menu... if you need to go elsewhere, override in child
        back_to_main_menu(button)

    def search_changed(self, widget, search_string):
        """ this gets called when search string changes """
        self.list_of_items_filtered = []
        self.search_phrase = search_string
        self.search_phrase_lc = search_string.lower()        # search string to lower case before search

        self.filter_items_by_search_phrase()

        # now set the 1st page and show page text
        self.page_current = 1
        self.last_focus_path = None
        self.show_page_text()                    # show the new page text
        self.update_pile_with_current_buttons()  # show the new buttons

    def filter_items_by_search_phrase(self):
        for item in self.list_of_items:                      # go through all items in list
            content = item['content'].lower()           # content to lower case

            if self.search_phrase_lc in content:             # if search string in content found
                self.list_of_items_filtered.append(item)     # append item

    def get_total_pages(self):
        total_items = len(self.list_of_items_filtered)                   # how many items current list has
        total_pages = math.ceil(total_items / shared.items_per_page)       # how many pages current list has
        return total_pages

    def show_page_text(self):
        total_pages = self.get_total_pages()     # how many pages current list has
        self.text_pages.set_text("{} / {}".format(self.page_current, total_pages))

    def on_page_change(self, change_direction):
        total_pages = self.get_total_pages()     # how many pages current list has

        if total_pages == 0:                # no total pages? no current page
            self.page_current = 1
        else:                               # some total pages?
            if change_direction > 0:                # to next page?
                if self.page_current < total_pages:      # not on last page yet? increment
                    self.page_current += 1
            else:                                   # to previous page?
                if self.page_current > 1:                # not on 1st page yet? decrement
                    self.page_current -= 1

        self.show_page_text()            # show the new page text

    def btn_prev_clicked(self, button):
        """ on prev page clicked """
        self.last_focus_path = None

        self.on_page_change(-1)
        self.update_pile_with_current_buttons()

    def btn_next_clicked(self, button):
        """ on next page clicked """
        self.last_focus_path = None

        self.on_page_change(+1)
        self.update_pile_with_current_buttons()

    def update_pile_with_current_buttons(self):
        """ the self.pile_current_page might be already shown on the screen,
            in that case we need to update the .contents attribude with tuples """
        current_page_buttons, focused_index = self.get_current_page_buttons(as_tuple=(self.pile_current_page is not None))

        if self.pile_current_page is None:       # no current pile? create pile from current page buttons
            self.pile_current_page = urwid.Pile(current_page_buttons)
        else:                               # we got the pile? update pile content with current page buttons
            self.pile_current_page.contents = current_page_buttons

        if focused_index is not None:       # got focused position? focus there
            self.pile_current_page.focus_position = focused_index

    def on_item_button_clicked(self, button, item):
        """ when user clicks on item button """
        # add implementation in child
        pass

    def get_current_page_buttons(self, as_tuple):
        """ create buttons for the current page number as a list
        as_tuple = False - for new pile, just list of buttons
        as_tuple = True  - for existing pile, as a list of tuples of buttons
        """
        from urwid.widget import WEIGHT

        sublist = self.get_list_for_current_page()       # get items for this page

        buttons = []

        focused_index = None

        for index, item in enumerate(sublist):      # from the items create buttons
            btn_text = self.get_item_btn_text(item)
            btn = create_my_button(btn_text, self.on_item_button_clicked, item)   # attach handler on clicked
            btn = (btn, (WEIGHT, 1)) if as_tuple else btn               # tuple or just the button
            buttons.append(btn)

        padding_cnt = shared.items_per_page - len(sublist)  # calculate how much padding at the bottom we need

        for i in range(padding_cnt):                # add all padding as needed
            one = (MyDivider(), (WEIGHT, 1)) if as_tuple else MyDivider()
            buttons.append(one)

        return buttons, focused_index

    def get_list_for_current_page(self):
        """ get one page from filtered list """
        self.page_current_zbi = (self.page_current - 1) if self.page_current > 1 else 0      # create zero-base index, but page_number could be 0
        item_start = self.page_current_zbi * shared.items_per_page

        sublist = self.list_of_items_filtered[item_start: (item_start + shared.items_per_page)]
        return sublist

    def get_item_btn_text(self, item):
        """ for a specified item retrieve full or partial content """
        # fill this in child
        return 'item text'

