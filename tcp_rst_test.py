
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
        "!HBBI",
        MAGIC,
        VERSION,
        MSG_TYPE,
        len(payload)
    ) + payload


def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(3)

    sock.connect((HOST, PORT))

    print("[RST] Connected")

    # 发送一条完整协议消息
    payload = b"RST_TEST_PAYLOAD"

    sock.sendall(make_packet(payload))

    print("[RST] Sent payload")

    # 设置 SO_LINGER:
    # l_onoff = 1
    # l_linger = 0
    #
    # close 时立即发送 RST
    sock.setsockopt(
        socket.SOL_SOCKET,
        socket.SO_LINGER,
        struct.pack("ii", 1, 0)
    )

    sock.close()

    print("[RST] Forced TCP reset")

    time.sleep(1)

    print("[PASS] RST test completed")


if __name__ == "__main__":
    main()

