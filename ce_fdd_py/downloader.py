import os
import urwid
import logging
import shared
from urwid_helpers import create_my_button, dialog, create_header_footer
from paginated_view import PaginatedView

app_log = logging.getLogger()


class DownloaderView(PaginatedView):
    list_index = 0
    list_of_items = []

    def on_show_selected_list(self, button, choice):
        self.list_index = choice     # store the chosen list index
        self.title = "CE FDD: " + shared.list_of_lists[shared.list_index]['name']

        # find out if we got storage path
        storage_path = shared.get_storage_path()

        if not storage_path:    # no storage path?
            shared.show_no_storage()
            return

        if not shared.can_write_to_storage(storage_path):
            shared.show_storage_read_only()
            return

        self.page_current = 1        # start from page 1
        self.search_phrase = ""      # no search string first
        self.last_focus_path = None

        # try to load the list into memory

        self.list_of_items = []

        error = None
        try:
            self.list_of_items = shared.load_list_from_csv(shared.list_of_lists[self.list_index]['filename'])
        except Exception as ex:
            error = str(ex)

        # first the filtered list is the same as original list
        self.list_of_items_filtered = self.list_of_items

        # if failed to load, show error
        if error:
            dialog(shared.main_loop, shared.current_body, f"Failed to load list!\n{error}")
            return

        shared.on_unhandled_keys_handler = self.on_unhandled_keys        # call this handler on arrows and other unhandled keys

        self.show_current_page(None)

    def on_unhandled_keys(self, key):
        """ when some key is not handled, this function is called """

        key = super().on_unhandled_keys(key)

        if not key:     # nothing more to handle? quit
            return

        widget = self.pile_current_page.focus            # get which image button has focus

        if not hasattr(widget, 'user_data'):        # no user data? unhandled key not on image button
            return

        user_data = widget.user_data
        filename = user_data['filename']     # get user data from that image button

        storage_path = shared.get_storage_path()           # check if got storage path

        if not storage_path:                        # no storage path? just quit
            return

        if not shared.can_write_to_storage(storage_path):  # check if can write to storage path
            return

        is_download = key in ['d', 'D']
        is_insert = key in ['1', '2', '3']

        path = os.path.join(storage_path, filename)     # check if got this file
        file_exists = os.path.exists(path)

        if is_insert and not file_exists:           # trying to insert, but don't have file?
            is_download = True                      # it's a download, not insert
            is_insert = False

        if is_download and file_exists:             # trying to download, but file exists? nothing do to
            return

        if is_insert:                               # should insert?
            slot = int(key) - 1
            self.insert_image(None, (path, slot))
            return

        if is_download:                             # should download?
            shared.queue_download.put(user_data)
            return

    def download_image(self, button, item):
        """ when image has been selected for download """
        app_log.debug("download image {}".format(item['filename']))
        shared.queue_download.put(item)        # enqueue image
        self.show_current_page(None)         # show current list page

    def insert_image(self, button, item_slot):
        """ when image should be inserted to slot """
        path_to_image, slot = item_slot
        shared.slot_insert(slot, path_to_image)
        app_log.debug(f"insert image {path_to_image} to slot {slot}")
        self.show_current_page(None)  # show current list page

    def on_item_button_clicked(self, button, item):
        """ when user clicks on image button """
        if self.main_list_pile is not None:
            self.last_focus_path = self.main_list_pile.get_focus_path()

        storage_path = shared.get_storage_path()           # check if got storage path

        if not storage_path:                        # no storage path?
            shared.show_no_storage()
            return

        if not shared.can_write_to_storage(storage_path):
            shared.show_storage_read_only()
            return

        path = os.path.join(storage_path, item['filename'])     # check if got this file

        header, footer = create_header_footer('>>> CosmosEx Floppy Tool <<<')
        body = []

        body.append(urwid.Text("file: " + item['filename'], align='center'))   # show filename

        body.append(urwid.Text("content: "))
        contents = item['content'].split(',')       # split csv content to list

        for content in contents:                    # show individual content items as rows
            body.append(urwid.Text("   " + content))

        body.append(urwid.Divider())

        if os.path.exists(path):                    # if exists, show insert options
            for i in range(3):
                slot = i + 1
                btn = create_my_button("Insert to slot {}".format(slot), self.insert_image, (path, i))
                body.append(btn)
        else:                                       # if doesn't exist, show download option
            btn = create_my_button("Download", self.download_image, item)
            body.append(btn)

        # add Back button
        btn = create_my_button("Back", self.show_current_page)
        body.append(btn)

        shared.main.original_widget = urwid.Frame(urwid.Filler(urwid.Pile(body)), header=header, footer=footer)

    def get_item_btn_text(self, item):
        """ for a specified item retrieve full or partial content """
        item_content = item['content']  # start with whole content

        if self.search_phrase:  # got search phrase? show only part of the content that matches
            content_parts = item['content'].split(',')  # split csv to list

            for part in content_parts:  # go through list
                part_lc = part.lower()  # create lowercase part

                if self.search_phrase in part_lc:  # if phrase found in lowercase part, use original part
                    item_content = part

        content_len = shared.terminal_cols - 25  # how long the content can be, when button sides and filename take some space
        item_content = item_content[:content_len]  # get only part of the content

        btn_text = "{:13} {}".format(item['filename'], item_content)  # format string as filename + content
        return btn_text
