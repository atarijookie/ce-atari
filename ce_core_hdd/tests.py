import socket
from time import sleep

def get_response(socket, expectCmd, expectLen):
    # wait for the correct starting byte
    while True:
        resp = socket.recv(1)

        if resp[0] == 0xc0:
            break

    # read additional 9 bytes to get whole 10 bytes of header
    resp = b'\xc0' + socket.recv(9)

    # extract sync, cmd, len
    sync = int.from_bytes(resp[0:4], "big")
    cmd = int.from_bytes(resp[4:6], "big")
    len = int.from_bytes(resp[6:10], "big")

    # print(''.join('{:02x}'.format(x) for x in resp))
    # print(f"sync: {sync:08x}, cmd: {cmd:04x}, len: {len:08x}")

    # validate sync, cmd, len
    if sync != SYNC_TAG_HDD:
        print("failed to get correct sync dword")
        exit(0)

    if expectCmd != cmd:
        print(f'wrong cmd: {cmd}')
        exit(0)

    if expectLen != len:
        print(f'wrong len: {len}')
        exit(0)

    # read the data
    resp = socket.recv(len)
    return resp


if __name__ == "__main__":
    print("Connecting to server")
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client.connect(('localhost', 8000))
    client.settimeout(0.5)

    # ------------------------------------------------------------------
    print("Test 1: send fw version")
    #                  | SYNC c050d1c5 | ATN 01 | LEN 12       |   2025  04  22    XLNX
    data = bytearray(b'\xc0\x50\xd1\xc5\x00\x01\x00\x00\x00\x0c\x00\x25\x04\x22\x00\x21\x00\x00\x00\x00\x00\x00')
    client.send(data)
    resp = get_response(client, 0x10, 2)
    print(f'resp: {resp}')
    

    sleep(3)
