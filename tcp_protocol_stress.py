
import socket
import struct
import hashlib
import threading
import time

HOST = "127.0.0.1"
PORT = 10000

MAGIC = 0x544C
VERSION = 1
MSG_TYPE = 1

FRAME_SIZE = 4096

def make_payload(index, length):
    return bytes((index + j) % 256 for j in range(length))

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
                f"Server disconnected: expected {size}, got {len(data)}"
            )

        data.extend(chunk)

    return bytes(data)

def run_test(total_size):

    print(f"\n========== TEST {total_size // 1024} KB ==========")

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(30)
    sock.connect((HOST, PORT))

    frame_count = (total_size + FRAME_SIZE - 1) // FRAME_SIZE

    send_hash = hashlib.sha256()
    recv_hash = hashlib.sha256()

    send_count = 0
    recv_count = 0

    errors = []

    # 发送线程
    def sender():
        nonlocal send_count

        try:
            remaining = total_size

            for index in range(frame_count):

                length = min(FRAME_SIZE, remaining)

                payload = make_payload(index, length)

                packet = make_packet(payload)

                sock.sendall(packet)

                send_hash.update(payload)

                send_count += len(payload)
                remaining -= length

        except Exception as e:
            errors.append(e)

    thread = threading.Thread(target=sender)
    thread.start()

    # 主线程持续接收
    try:
        for index in range(frame_count):

            header = recv_exact(sock, 8)

            magic, version, msg_type, length = struct.unpack(
                "!HBBI", header
            )

            assert magic == MAGIC, f"Invalid MAGIC: {magic:#x}"
            assert version == VERSION
            assert msg_type == MSG_TYPE

            expected_length = min(
                FRAME_SIZE,
                total_size - index * FRAME_SIZE
            )

            assert length == expected_length, (
                f"Invalid length: {length}, "
                f"expected {expected_length}"
            )

            payload = recv_exact(sock, length)

            expected_payload = make_payload(index, length)

            assert payload == expected_payload, (
                f"Payload mismatch at frame {index}"
            )

            recv_hash.update(payload)
            recv_count += len(payload)

    finally:
        thread.join()
        sock.close()

    if errors:
        raise errors[0]

    send_digest = send_hash.hexdigest()
    recv_digest = recv_hash.hexdigest()

    print("[SEND] Bytes:", send_count)
    print("[RECV] Bytes:", recv_count)
    print("[SEND] SHA256:", send_digest)
    print("[RECV] SHA256:", recv_digest)

    assert send_count == total_size
    assert recv_count == total_size
    assert send_digest == recv_digest

    print("[PASS] Protocol stress test")


if __name__ == "__main__":

    for size in [
        1 * 1024 * 1024,
        5 * 1024 * 1024,
        10 * 1024 * 1024
    ]:
        run_test(size)

    print("\nALL PROTOCOL STRESS TESTS PASS")

