#!/usr/bin/env python

import asyncio
import logging
import json
import websockets


logging.basicConfig(
    format="%(message)s",
    level=logging.DEBUG,
)

exchange = "deribit"
symbol = "BTC-PERPETUAL"

logon = dict(
    username="trader",
    password="secret",
)

order_mass_status_request = dict(
    mass_status_req_id="test_000",
)

new_order_single = dict(
    cl_ord_id="test_001",
    exchange=exchange,
    symbol=symbol,
    side="BUY",
    ord_type="LIMIT",
    time_in_force="GTC",
    quantity=1.0,
    price=123.45,
)

order_cancel_request = dict(
    orig_cl_ord_id="test_001",
    cl_ord_id="test_002",
    exchange=exchange,
    symbol=symbol,
)

order_mass_cancel_request = dict()

logout = dict()


def create_request(method, params, id):
    request = dict(
        jsonrpc="2.0",
        method=method,
        params=params,
        id=id,
    )
    return json.dumps(request)


async def hello():
    async with websockets.connect("ws://localhost:2345") as ws:
        try:
            await ws.send(create_request("logon", logon, 1000))
            msg = await ws.recv()
            # TODO check status
            # ... now we could receive interleaved notifications
            await ws.send(
                create_request(
                    "order_mass_status_request", order_mass_status_request, 1001
                )
            )
            msg = await ws.recv()
            # TODO check status
            # TODO several execution reports
            await ws.send(create_request("new_order_single", new_order_single, 1002))
            msg = await ws.recv()
            # TODO check status
            await ws.send(
                create_request("order_cancel_request", order_cancel_request, 1003)
            )
            msg = await ws.recv()
            # TODO check status
            await ws.send(
                create_request(
                    "order_mass_cancel_request", order_mass_cancel_request, 1004
                )
            )
            msg = await ws.recv()
            # TODO check status
            await ws.send(create_request("logout", logout, 1005))
            msg = await ws.recv()
            # TODO check status
        except websockets.ConnectionClosedOK:
            print("closed ok")
        except websockets.ConnectionClosedError:
            print("closed error")
        except websockets.ConnectionClosed:
            print("closed")
        print("done")


async def handler(websocket):
    while True:
        try:
            message = await websocket.recv()
        except websockets.ConnectionClosedOK:
            break
        print(message)


if __name__ == "__main__":
    asyncio.run(hello())
