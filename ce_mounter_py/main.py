from setproctitle import setproctitle
import logging
from logging.handlers import RotatingFileHandler


if __name__ == "__main__":
    setproctitle("ce_mounter")  # set process title

    log_formatter = logging.Formatter('%(asctime)s %(levelname)s %(funcName)s(%(lineno)d) %(message)s')

    my_handler = RotatingFileHandler('/tmp/ce_mounter.log', mode='a', maxBytes=1024 * 1024, backupCount=1)
    my_handler.setFormatter(log_formatter)
    my_handler.setLevel(logging.DEBUG)

    app_log = logging.getLogger()
    app_log.setLevel(logging.DEBUG)
    app_log.addHandler(my_handler)

