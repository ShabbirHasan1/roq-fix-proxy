#!/usr/bin/env python

import asyncio
import logging
import websockets


logging.basicConfig(
    format="%(message)s",
    level=logging.DEBUG,
)


async def hello():
    async with websockets.connect("ws://localhost:2345", compression=None) as ws:
        try:
            await ws.send(
                '{"jsonrpc":"2.0","method":"new_order_single","params":{"symbol":"BTC-PERPETUAL"},"id":"1234"}'
            )
            msg = await ws.recv()
            print(msg)
            # await ws.send('{"action":"subscribe","symbol":"ETH-PERPETUAL"}')
            # msg = await ws.recv()
            # print(msg)
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
