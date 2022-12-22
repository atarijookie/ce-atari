import os

LOG_DIR = '/var/log/ce/'
SETTINGS_PATH = '/ce/settings'

DATA_DIR = '/var/run/ce/'
PID_FILE = os.path.join(DATA_DIR, 'ce_webserver.pid')

SECRET_KEY_PATH = os.path.join(DATA_DIR, "flask_secret_key")

FILE_STATUS = '/tmp/UPDATE_STATUS'
FILE_PENDING_YES = '/tmp/UPDATE_PENDING_YES'
FILE_PENDING_NO = '/tmp/UPDATE_PENDING_NO'

SOCK_PATH_CONFIG = '/var/run/ce/app0.sock'
SOCK_PATH_FLOPPY = '/var/run/ce/app1.sock'
SOCK_PATH_TERMINAL = '/var/run/ce/app2.sock'

PATH_TO_LISTS = "/ce/lists/"                                    # where the lists are stored locally
BASE_URL = "http://joo.kie.sk/cosmosex/update/"                 # base url where the lists will be stored online
LIST_OF_LISTS_FILE = "list_of_lists.csv"
LIST_OF_LISTS_URL = BASE_URL + LIST_OF_LISTS_FILE               # where the list of lists is on web
LIST_OF_LISTS_LOCAL = PATH_TO_LISTS + LIST_OF_LISTS_FILE        # where the list of lists is locally

CORE_SOCK_PATH = os.path.join(DATA_DIR, 'core.sock')

FILE_FLOPPY_SLOTS = os.path.join(DATA_DIR, 'floppy_slots.txt')
FILE_FLOPPY_ACTIVE_SLOT = os.path.join(DATA_DIR, 'floppy_active_slot.txt')

DOWNLOAD_STORAGE_DIR = os.path.join(DATA_DIR, 'download_storage')
last_storage_path = None

FLOPPY_UPLOAD_PATH = os.path.join(DATA_DIR, 'floppy_slots')
