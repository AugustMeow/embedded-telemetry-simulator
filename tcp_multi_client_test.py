
import socket
import struct
import threading
import time

HOST = "127.0.0.1"
PORT = 10000

MAGIC = 0x544C
VERSION = 1
MSG_TYPE = 1

def make_packet(payload):
    header = struct.pack(
        "!HBBI",
        MAGIC,
        VERSION,
        MSG_TYPE,
        len(payload)
    )
    return header + payload

def recv_exact(sock, size):
    data = b""

    while len(data) < size:
        chunk = sock.recv(size - len(data))

        if not chunk:
            raise RuntimeError("Server disconnected")

        data += chunk

    return data

def recv_packet(sock):
    header = recv_exact(sock, 8)

    magic, version, msg_type, length = struct.unpack(
        "!HBBI", header
    )

    assert magic == MAGIC
    assert version == VERSION
    assert msg_type == MSG_TYPE
    assert length <= 4096

    payload = recv_exact(sock, length)

    return payload

def client_a():
    payload = b"HELLO_A_12345"
    packet = make_packet(payload)

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((HOST, PORT))

    # 第一部分：只发送 Header 前 3 字节
    sock.sendall(packet[:3])
    print("[A] Sent part 1")

    time.sleep(0.3)

    # 第二部分：发送剩余数据
    sock.sendall(packet[3:])
    print("[A] Sent part 2")

    response = recv_packet(sock)

    print("[A] Received:", response)

    assert response == payload

    print("[A] PASS")

    sock.close()

def client_b():
    payload = b"WORLD_B_67890"
    packet = make_packet(payload)

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((HOST, PORT))

    # 一次发送完整消息
    sock.sendall(packet)
    print("[B] Sent complete packet")

    response = recv_packet(sock)

    print("[B] Received:", response)

    assert response == payload

    print("[B] PASS")

    sock.close()


# 两个客户端并发运行
thread_a = threading.Thread(target=client_a)
thread_b = threading.Thread(target=client_b)

thread_a.start()

time.sleep(0.1)

thread_b.start()

thread_a.join()
thread_b.join()

print("TCP multi-client isolation test PASS")

