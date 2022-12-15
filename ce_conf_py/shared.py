import os

terminal_cols = 80  # should be 40 for ST low, 80 for ST mid
terminal_rows = 23
items_per_page = 19

main = None
main_loop = None
current_body = None
should_run = True

settings = {}
settings_changed = {}

LOG_DIR = '/var/log/ce/'
SETTINGS_PATH = '/ce/settings'

DATA_DIR = '/var/run/ce/'
PID_FILE = os.path.join(DATA_DIR, 'ce_config.pid')
