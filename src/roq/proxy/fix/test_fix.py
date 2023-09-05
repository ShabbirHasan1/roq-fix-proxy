#!/usr/bin/env python

from datetime import datetime

import os
import socket

import simplefix

FIX="FIX.4.4"

SENDER_COMP_ID = "test"
TARGET_COMP_ID= "proxy"

USERNAME = "trader"
PASSWORD = "secret"

EXCHANGE = "deribit"
SYMBOL = "BTC-PERPETUAL"

def logon_request():
    msg = simplefix.FixMessage()
    msg.append_pair(8, FIX)
    msg.append_pair(35, 'A')
    msg.append_pair(49, SENDER_COMP_ID)
    msg.append_pair(56, TARGET_COMP_ID)
    msg.append_pair(553, USERNAME)
    msg.append_pair(554, PASSWORD)
    request = msg.encode()
    print(request)
    return request

def new_order_single_request():
    msg = simplefix.FixMessage()
    msg.append_pair(8, FIX)
    msg.append_pair(35, 'D')
    msg.append_pair(49, SENDER_COMP_ID)
    msg.append_pair(56, TARGET_COMP_ID)
    msg.append_pair(11, "clordid1")
    msg.append_pair(55, SYMBOL)
    msg.append_pair(207, EXCHANGE)
    msg.append_pair(54, '1')  # buy
    msg.append_pair(40, '2')  # limit
    msg.append_pair(38, 1.0)  # quantity
    msg.append_pair(44, 100.0)  # price
    msg.append_pair(59, '1')  # gtc
    request = msg.encode()
    print(request)
    return request

if __name__ == "__main__":
    home = os.getenv('HOME')
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
        s.connect('{}/run/fix-proxy.sock'.format(home))
        s.sendall(logon_request())
        response = s.recv(4096)
        print(response)
        s.sendall(new_order_single_request())
        response = s.recv(4096)
        print(response)
