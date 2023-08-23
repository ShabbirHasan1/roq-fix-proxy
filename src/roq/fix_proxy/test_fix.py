#!/usr/bin/env python

from datetime import datetime

import socket

import simplefix

if __name__ == "__main__":
    msg = simplefix.FixMessage()
    msg.append_pair(8, 'FIX.4.4')
    msg.append_pair(35, 'A')
    msg.append_pair(49, 'me')
    msg.append_pair(56, 'proxy')
    msg.append_utc_timestamp(52, precision=6, header=True)
    msg.append_pair(553, 'user')
    msg.append_pair(554, 'pwd')
    request = msg.encode()
    print(len(request))
    print(request)
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect(('localhost', 1234))
        s.sendall(request)
        response = s.recv(4096)
        print(response)
