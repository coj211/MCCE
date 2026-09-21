# -*- coding: utf-8 -*-
"""探测 Minecraft PE 服务器：发一个 RakNet unconnected ping，打印它广播的服务器名。

用途：改完 server-name 后不用开客户端就能验证"客户端服务器列表里会显示什么"。

用法：python tools/ping_server.py [host] [port]
"""
import socket
import struct
import sys
import time

# RakNet offline message magic（固定 16 字节）
MAGIC = bytes.fromhex("00ffff00fefefefefdfdfdfd12345678")

ID_UNCONNECTED_PING = 0x01
ID_UNCONNECTED_PONG = 0x1C


def main():
    host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 19132

    ts = int(time.time())
    # RakNet 各版本的 unconnected ping 长得不一样，逐个试：
    #   0x01 = ID_UNCONNECTED_PING，0x02 = ID_UNCONNECTED_PING_OPEN_CONNECTIONS
    #   带不带 clientGuid 也因版本而异
    candidates = [
        (0x01, True), (0x02, True), (0x01, False), (0x02, False),
    ]

    data = None
    for msg_id, with_guid in candidates:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.settimeout(1.5)
        payload = bytes([msg_id]) + struct.pack(">Q", ts) + MAGIC
        if with_guid:
            payload += struct.pack(">Q", 0x1122334455667788)
        s.sendto(payload, (host, port))
        try:
            data, addr = s.recvfrom(4096)
            s.close()
            print("[ping 格式 id=0x%02x guid=%s 有响应]" % (msg_id, with_guid))
            break
        except socket.timeout:
            data = None
            s.close()

    if data is None:
        print("超时：%s:%d 对 4 种 RakNet ping 格式都没响应"
              "（服务器没开？端口不对？）" % (host, port))
        return 1

    if not data or data[0] != ID_UNCONNECTED_PONG:
        print("收到非 pong 响应: %s" % data[:8].hex())
        return 1

    # pong = 0x1c + timestamp(8) + serverGUID(8) + magic(16) + string(2字节长度+内容)
    off = 1 + 8 + 8 + 16
    strlen = struct.unpack(">H", data[off:off + 2])[0]
    text = data[off + 2:off + 2 + strlen].decode("utf-8", "replace")
    print("来自 %s:%d" % (host, port))
    print("  原始广播串 : %r" % text)
    # 形如 MCCPP;<version>;<server name>
    parts = text.split(";")
    print("  客户端看到的名字 : %r" % (parts[-1] if parts else text))
    return 0


if __name__ == "__main__":
    sys.exit(main())
