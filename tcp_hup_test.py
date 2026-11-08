
import socket
import struct
import time

HOST = "127.0.0.1"
PORT = 10000

MAGIC = 0x544C
VERSION = 1
MSG_TYPE = 1


def make_packet(payload):
    return struct.pack(
        "!HBBI", MAGIC, VERSION, MSG_TYPE, len(payload)
    ) + payload


def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    sock.connect((HOST, PORT))

    print("[B] Connected")

    # 发送完整协议帧
    sock.sendall(make_packet(b"HELLO_HUP"))
    print("[B] Sent HELLO_HUP")

    # 直接关闭整个连接，不执行 shutdown
    sock.close()

    print("[B] Socket closed")

    # 留出时间让服务端处理 FIN/HUP
    time.sleep(2)

    print("[PASS] HUP client test completed")


if __name__ == "__main__":
    main()

