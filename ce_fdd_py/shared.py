import queue

queue_download = queue.Queue()      # queue that holds things to download

terminal_cols = 80  # should be 40 for ST low, 80 for ST mid
terminal_rows = 23
items_per_page = 19

main = None
main_loop = None
current_body = None
should_run = True

PATH_VAR = '/var/run/ce'
FILE_SLOTS = PATH_VAR + '/slots.txt'

PATH_TO_LISTS = "/ce/lists/"                                    # where the lists are stored locally
BASE_URL = "http://joo.kie.sk/cosmosex/update/"                 # base url where the lists will be stored online
LIST_OF_LISTS_FILE = "list_of_lists.csv"
LIST_OF_LISTS_URL = BASE_URL + LIST_OF_LISTS_FILE               # where the list of lists is on web
LIST_OF_LISTS_LOCAL = PATH_TO_LISTS + LIST_OF_LISTS_FILE        # where the list of lists is locally

list_of_lists = []      # list of dictionaries: {name, url, filename}
list_index = 0          # index of list in list_of_lists which will be worked on
list_of_items = []      # list containing all the items from file (unfiltered)
list_of_items_filtered = []     # filtered list of items (based on search string)
pile_current_page = None        # urwid pile containing buttons for current page
search_phrase = ""

main_loop = None        # main loop of the urwid library
text_pages = None       # widget holding the text showing current and total pages
page_current = 1        # currently shown page
text_status = None      # widget holding status text
main_list_pile = None   # the main pile containing buttons with images
last_focus_path = None  # holds last focus path to widget which had focus before going to widget subpage
terminal_cols = 80      # should be 40 for ST low, 80 for ST mid
terminal_rows = 23
items_per_page = terminal_rows - 4
last_storage_path = None

on_unhandled_keys_handler = None


def on_unhandled_keys_generic(key):
    # generic handler for unhandled keys, will be assigned on start...
    # later on, when you want to handle unhandled keys on specific screen, set the on_unhandled_keys_handler
    # to handling function

    if on_unhandled_keys_handler:
        on_unhandled_keys_handler(key)
