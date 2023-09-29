#!/usr/bin/env python

from datetime import datetime

import os
import socket

import simplefix

FIX = "FIX.4.4"

SENDER_COMP_ID = "test"
TARGET_COMP_ID = "proxy"

USERNAME = "trader"
PASSWORD = "secret"

ACCOUNT = "A1"
EXCHANGE = "deribit"
SYMBOL = "BTC-PERPETUAL"


def logon_request():
    msg = simplefix.FixMessage()
    msg.append_pair(8, FIX)
    msg.append_pair(35, "A")
    msg.append_pair(49, SENDER_COMP_ID)
    msg.append_pair(56, TARGET_COMP_ID)
    msg.append_pair(553, USERNAME)
    msg.append_pair(554, PASSWORD)
    request = msg.encode()
    print(request)
    return request


def logout_request():
    msg = simplefix.FixMessage()
    msg.append_pair(8, FIX)
    msg.append_pair(35, "5")
    msg.append_pair(49, SENDER_COMP_ID)
    msg.append_pair(56, TARGET_COMP_ID)
    request = msg.encode()
    print(request)
    return request


def market_data_request():
    msg = simplefix.FixMessage()
    msg.append_pair(8, FIX)
    msg.append_pair(35, "V")
    msg.append_pair(49, SENDER_COMP_ID)
    msg.append_pair(56, TARGET_COMP_ID)
    msg.append_pair(262, "test")
    msg.append_pair(263, 1)  # snapshot+updates
    msg.append_pair(264, 0)  # full book
    msg.append_pair(265, 1)  # incremental
    msg.append_pair(266, "Y")  # aggregated book
    msg.append_pair(267, 2)  # NoMDEntryTypes
    msg.append_pair(269, 0)  # bid
    msg.append_pair(269, 1)  # ask
    msg.append_pair(146, 1)  # NoRelatedSym
    msg.append_pair(55, "BTC-PERPETUAL")
    msg.append_pair(207, "deribit")
    request = msg.encode()
    print(request)
    return request


def new_order_single_request():
    msg = simplefix.FixMessage()
    msg.append_pair(8, FIX)
    msg.append_pair(35, "D")
    msg.append_pair(49, SENDER_COMP_ID)
    msg.append_pair(56, TARGET_COMP_ID)
    msg.append_pair(11, "clordid1")
    msg.append_pair(55, SYMBOL)
    msg.append_pair(207, EXCHANGE)
    msg.append_pair(54, "1")  # buy
    msg.append_pair(40, "2")  # limit
    msg.append_pair(38, 1.0)  # quantity
    msg.append_pair(44, 100.0)  # price
    msg.append_pair(59, "1")  # gtc
    request = msg.encode()
    print(request)
    return request


# 8=FIX.4.4|9=96|35=AN|34=8|49=test|52=20230928-16:42:00.000|56=proxy|1=A1|207=deribit|581=1|710=pos_00002|724=0|10=10
def request_for_positions_request():
    msg = simplefix.FixMessage()
    msg.append_pair(8, FIX)
    msg.append_pair(35, "AN")
    msg.append_pair(49, SENDER_COMP_ID)
    msg.append_pair(56, TARGET_COMP_ID)
    msg.append_pair(1, ACCOUNT)
    msg.append_pair(207, EXCHANGE)
    msg.append_pair(581, "1")
    msg.append_pair(710, "pos_00002")
    msg.append_pair(724, "0")
    request = msg.encode()
    print(request)
    return request


if __name__ == "__main__":
    home = os.getenv("HOME")
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
        s.connect("{}/run/fix-proxy.sock".format(home))
        # logon
        s.sendall(logon_request())
        response = s.recv(4096)
        print(response)
        # request for positions
        s.sendall(request_for_positions_request())
        response = s.recv(4096)
        print(response)
        # s.sendall(logout_request())
        # response = s.recv(4096)
        # print(response)
        if False:
            s.sendall(new_order_single_request())
        else:
            s.sendall(market_data_request())
        while True:
            response = s.recv(4096)
            if len(response) > 0:
                print(response)
