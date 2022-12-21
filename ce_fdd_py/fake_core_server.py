import socket
import sys
import os
import base64
import json
from shared import FILE_FLOPPY_SLOTS, FILE_FLOPPY_ACTIVE_SLOT, CORE_SOCK_PATH

slots = {}


def text_to_file(text, filename):
    # write text to file for later use
    try:
        with open(filename, 'wt') as f:
            f.write(text)
    except Exception as ex:
        app_log.warning(logging.WARNING, f"failed to write to {filename}: {str(ex)}")


def write_slots_to_file():
    with open(FILE_FLOPPY_SLOTS, 'wt') as f:
        for i in range(3):
            img = slots.get(i, '')
            f.write(img)
            f.write('\n')


def create_socket():
    try:
        os.unlink(CORE_SOCK_PATH)
    except:
        if os.path.exists(CORE_SOCK_PATH):
            raise

    sckt = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)

    try:
        sckt.bind(CORE_SOCK_PATH)
        print(f'Success, got socket: {CORE_SOCK_PATH}')
        return sckt
    except Exception as e:
        print(f'exception on bind: {str(e)}')
        return False


if __name__ == "__main__":
    sock = create_socket()

    if not sock:
        exit(0)

    while True:
        # connection, client_address = sock.accept()

        try:
            buffer = b''
            rcvd_bytes = 0

            data, address = sock.recvfrom(1024)
            decoded = json.loads(data)
            print(f'received: {decoded}')

            # example message: {'module': 'floppy', 'action': 'eject', 'slot': 0}
            if decoded.get('module') == 'floppy':       # command for floppy module?
                slot = decoded.get('slot')

                if decoded.get('action') == 'eject':    # action is eject?
                    slots.pop(slot, None)
                    write_slots_to_file()

                if decoded.get('action') == 'insert':    # action is insert?
                    slots[slot] = decoded.get('image')
                    write_slots_to_file()

                if decoded.get('action') == 'activate':  # action is activate?
                    text_to_file(f"{slot}", FILE_FLOPPY_ACTIVE_SLOT)

        except Exception as ex:
            print(f"got exception: {str(ex)}")
