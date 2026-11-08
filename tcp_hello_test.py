
import socket
import struct
import hashlib

HOST = "127.0.0.1"
PORT = 10000

MAGIC = 0x544C
VERSION = 1
MSG_TYPE = 1

PAYLOAD = b"HELLO"


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
    data = bytearray()

    while len(data) < size:
        chunk = sock.recv(size - len(data))

        if not chunk:
            raise RuntimeError(
                f"Server disconnected: "
                f"expected={size}, received={len(data)}"
            )

        data.extend(chunk)

    return bytes(data)


def main():

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)

    try:
        sock.connect((HOST, PORT))
        print("[INFO] Connected to server")

        # 1. 发送 HELLO 协议帧
        packet = make_packet(PAYLOAD)

        print("[SEND] Header:", packet[:8].hex(" "))
        print("[SEND] Payload:", PAYLOAD)

        sock.sendall(packet)

        # 2. 接收响应 Header
        header = recv_exact(sock, 8)

        magic, version, msg_type, length = struct.unpack(
            "!HBBI", header
        )

        print("[RECV] Magic:", hex(magic))
        print("[RECV] Version:", version)
        print("[RECV] Type:", msg_type)
        print("[RECV] Length:", length)

        # 3. 验证 Header
        assert magic == MAGIC
        assert version == VERSION
        assert msg_type == MSG_TYPE
        assert length == len(PAYLOAD)

        # 4. 接收响应 Payload
        response = recv_exact(sock, length)

        print("[RECV] Payload:", response)

        # 5. 验证数据完整性
        assert response == PAYLOAD

        send_hash = hashlib.sha256(PAYLOAD).hexdigest()
        recv_hash = hashlib.sha256(response).hexdigest()

        print("[SEND] SHA256:", send_hash)
        print("[RECV] SHA256:", recv_hash)

        assert send_hash == recv_hash

        print("[PASS] TCP HELLO echo test")

    finally:
        sock.close()


if __name__ == "__main__":
    main()

