import os

LOG_DIR = '/var/log/ce/'
SETTINGS_PATH = '/ce/settings'

DATA_DIR = '/var/run/ce/'
PID_FILE = os.path.join(DATA_DIR, 'ce_webserver.pid')

FILE_STATUS = '/tmp/UPDATE_STATUS'
FILE_PENDING_YES = '/tmp/UPDATE_PENDING_YES'
FILE_PENDING_NO = '/tmp/UPDATE_PENDING_NO'

