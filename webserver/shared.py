import os

LOG_DIR = '/var/log/ce/'
SETTINGS_PATH = '/ce/settings'

DATA_DIR = '/var/run/ce/'
PID_FILE = os.path.join(DATA_DIR, 'ce_webserver.pid')

FILE_STATUS = '/tmp/UPDATE_STATUS'
FILE_PENDING_YES = '/tmp/UPDATE_PENDING_YES'
FILE_PENDING_NO = '/tmp/UPDATE_PENDING_NO'

SOCK_PATH_CONFIG = '/var/run/ce/app0.sock'
SOCK_PATH_FLOPPY = '/var/run/ce/app1.sock'
SOCK_PATH_TERMINAL = '/var/run/ce/app2.sock'
