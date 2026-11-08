
import socket
import struct
import threading
import hashlib
import time

HOST = "127.0.0.1"
PORT = 10000

MAGIC = 0x544C
VERSION = 1
MSG_TYPE = 1

TOTAL_SIZE = 1024 * 1024
FRAME_SIZE = 4096


def make_payload(index, length):
    return bytes((index + j) % 256 for j in range(length))


def make_packet(payload):
    return struct.pack(
        "!HBBI",
        MAGIC, VERSION, MSG_TYPE, len(payload)
    ) + payload


def recv_exact(sock, size):
    data = bytearray()

    while len(data) < size:
        chunk = sock.recv(size - len(data))

        if not chunk:
            raise RuntimeError("Server disconnected")

        data.extend(chunk)

    return bytes(data)


def main():

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(30)
    sock.connect((HOST, PORT))

    frame_count = TOTAL_SIZE // FRAME_SIZE

    send_errors = []
    send_hash = hashlib.sha256()
    recv_hash = hashlib.sha256()

    def sender():
        try:
            for index in range(frame_count):
                payload = make_payload(index, FRAME_SIZE)
                sock.sendall(make_packet(payload))
                send_hash.update(payload)

            print("[SENDER] Completed")

        except Exception as e:
            send_errors.append(e)
            print("[SENDER ERROR]", repr(e))

    thread = threading.Thread(target=sender)
    thread.start()

    # 故意暂时不读取服务端响应
    print("[INFO] Pause receiving for 2 seconds")
    time.sleep(2)

    print("[INFO] Start receiving")

    recv_count = 0

    try:
        for index in range(frame_count):

            header = recv_exact(sock, 8)

            magic, version, msg_type, length = struct.unpack(
                "!HBBI", header
            )

            assert magic == MAGIC
            assert version == VERSION
            assert msg_type == MSG_TYPE
            assert length == FRAME_SIZE

            payload = recv_exact(sock, length)

            expected = make_payload(index, length)
            assert payload == expected

            recv_hash.update(payload)
            recv_count += length

    finally:
        thread.join()
        sock.close()

    if send_errors:
        raise send_errors[0]

    print("[RECV] Bytes:", recv_count)
    print("[SEND] SHA256:", send_hash.hexdigest())
    print("[RECV] SHA256:", recv_hash.hexdigest())

    assert recv_count == TOTAL_SIZE
    assert send_hash.digest() == recv_hash.digest()

    print("[PASS] High/Low Watermark test")


if __name__ == "__main__":
    main()

