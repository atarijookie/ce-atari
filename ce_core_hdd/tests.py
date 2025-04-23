import socket
from time import sleep

if __name__ == "__main__":
    print("Connecting to server")
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client.connect(('localhost', 8000))

    # ------------------------------------------------------------------
    print("Test 1: send fw version")
    #                                   HDR
    #                  | SYNC c050d1c5 | ATN 01 | LEN 12       |   2025  04  22    XLNX
    data = bytearray(b'\xc0\x50\xd1\xc5\x00\x01\x00\x00\x00\x0c\x00\x25\x04\x22\x00\x21\x00\x00\x00\x00\x00\x00')
    client.send(data)

    sleep(3)
