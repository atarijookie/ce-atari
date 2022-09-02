import os
import urwid
import logging
import shared
from urwid_helpers import dialog, back_to_main_menu
from paginated_view import PaginatedView

app_log = logging.getLogger()


class FilesView(PaginatedView):
    path = 0
    list_of_items = []
    fdd_slot = 0

    def set_fdd_slot(self, slot):
        self.fdd_slot = slot

    def get_file_size(self, path, is_file):
        if not is_file:
            return '    (dir)'

        file_stats = os.stat(path)
        size = file_stats.st_size
        size_prev = size

        units = ['B', 'B', 'kB', 'MB', 'GB', 'TB']
        for unit in units:                          # go through the units
            if size < 1.0:                          # size value is less than 1? use previous size value
                if unit != 'B':     # for kB / MB / GB
                    size_int = int(size_prev)
                    size_str = f"{size_int} {unit}"
                else:               # for B
                    size_str = f"{size_prev}  {unit}"

                return f"{size_str:>9}"     # align right, pad from left with spaces

            size_prev = size            # keep previous size value
            size = size / 1024          # divide by 1024 to get new size value

        return '???'        # this should never happen

    def on_show_selected_list(self, button, choice):
        self.path = choice          # store the chosen list index
        self.title = "CE FDD: " + self.path

        self.page_current = 1        # start from page 1
        self.search_phrase = ""      # no search string first
        self.last_focus_path = None

        # try to load the list into memory
        self.list_of_items = []

        files = []
        dirs = []

        if self.path != '/':        # if it's not the root dir, include up dir (..)
            up_path = os.path.dirname(self.path)        # get one level up dir name
            dirs.append({'full_path': up_path, 'filename': '..', 'filename_lc': '..',
                         'is_file': False, 'size': self.get_file_size(up_path, False)})

        error = None
        try:    # find all the files and dirs in this dir
            for path in os.listdir(self.path):
                full_path = os.path.join(self.path, path)
                is_file = os.path.isfile(full_path)
                file_size = self.get_file_size(full_path, is_file)

                item = {'full_path': full_path, 'filename': path, 'filename_lc': path.lower(),
                        'is_file': is_file, 'size': file_size}

                if is_file:             # files to one list
                    files.append(item)
                else:                   # dirs to another list
                    dirs.append(item)
        except Exception as ex:
            error = str(ex)

        # sort the dirs and files by filename
        def get_key_fun(e):
            return e['filename'].lower()

        dirs.sort(key=get_key_fun)
        files.sort(key=get_key_fun)

        self.list_of_items = dirs + files       # first dirs, then files

        # first the filtered list is the same as original list
        self.list_of_items_filtered = self.list_of_items

        # if failed to load, show error
        if error:
            dialog(shared.main_loop, shared.current_body, f"Failed to load directory!\n{error}")
            return

        # call this handler on arrows and other unhandled keys
        shared.on_unhandled_keys_handler = self.on_unhandled_keys
        self.show_current_page(None)

    def on_item_button_clicked(self, button, item):
        """ when user clicks on image button """
        if self.main_list_pile is not None:
            self.last_focus_path = self.main_list_pile.get_focus_path()

        path = item['full_path']     # check if got this file

        if item['is_file']:         # is file? insert
            # check if this is a valid image
            is_image, error_str = shared.file_seems_to_be_image(path, True)

            if not is_image:        # if not valid image
                dialog(shared.main_loop, shared.current_body, error_str)
                return

            # insert image into slot
            shared.slot_insert(self.fdd_slot, path)
            app_log.debug(f"insert image {path} to slot {self.fdd_slot}")

            from image_slots import on_show_image_slots
            on_show_image_slots(button)
        else:                       # is dir? browse
            self.on_show_selected_list(button, path)

    def get_item_btn_text(self, item):
        """ for a specified item retrieve text """
        btn_text = f"{item['filename']:13} {item['size']}"
        return btn_text

    def on_back_button(self, button):
        from image_slots import on_show_image_slots
        on_show_image_slots(button)

    def filter_items_by_search_phrase(self):
        for item in self.list_of_items:                     # go through all items in list
            content = item['filename'].lower()              # content to lower case

            if self.search_phrase_lc in content:            # if search string in content found
                self.list_of_items_filtered.append(item)    # append item
