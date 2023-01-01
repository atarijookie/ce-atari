terminal_cols = 80  # should be 40 for ST low, 80 for ST mid
terminal_rows = 23
items_per_page = 19

main = None
main_loop = None
current_body = None
should_run = True

settings = {}
settings_changed = {}

# for saving things that take longer time
thread_save_running = False
thread_save = None
