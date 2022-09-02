import socket
import sys
import os
import base64
import bson


slots = {}

PATH_VAR = '/var/run/ce'
FILE_SLOTS = PATH_VAR + '/slots.txt'


def write_slots_to_file():
    with open(FILE_SLOTS, 'wt') as f:
        for i in range(3):
            img = slots.get(i, '')
            f.write(img)
            f.write('\n')


def prep():
    srv_sock = '/var/run/ce/core.sock'
    try:
        os.unlink(srv_sock)
    except:
        if os.path.exists(srv_sock):
            raise

    sckt = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

    try:
        print(f'Bind and listen on socket: {srv_sock}')
        sckt.bind(srv_sock)
        sckt.listen(5)
        print(f'Success, listening on: {srv_sock}')
        return sckt
    except Exception as e:
        print(f'exception on bind or listen: {str(e)}')
        return False


if __name__ == "__main__":
    sock = prep()

    if not sock:
        exit(0)

    while True:
        connection, client_address = sock.accept()

        try:
            buffer = b''
            rcvd_bytes = 0

            while True:
                data = connection.recv(1024)
                rcvd_bytes = rcvd_bytes + len(data)
                if not data:        # on connection close
                    break

                buffer = buffer + data

            decoded = bson.loads(buffer)
            print(f'received: {decoded}')

            # {'module': 'floppy', 'action': 'eject', 'slot': 0}
            if decoded.get('module') == 'floppy':       # command for floppy module?
                slot = decoded.get('slot')

                if decoded.get('action') == 'eject':    # action is eject?
                    slots.pop(slot, None)
                    write_slots_to_file()

                if decoded.get('action') == 'insert':    # action is insert?
                    slots[slot] = decoded.get('image')
                    write_slots_to_file()

        finally:
            connection.close()
