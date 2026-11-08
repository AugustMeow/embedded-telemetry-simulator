
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

TOTAL_SIZE = 1 * 1024 * 1024
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
                f"Server disconnected: "
                f"expected={size}, received={len(data)}"
            )

        data.extend(chunk)

    return bytes(data)


def main():

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(30)
    sock.connect((HOST, PORT))

    frame_count = (TOTAL_SIZE + FRAME_SIZE - 1) // FRAME_SIZE

    send_hash = hashlib.sha256()
    recv_hash = hashlib.sha256()

    send_count = 0
    recv_count = 0

    send_errors = []

    print("[INFO] Connected to server")
    print("[INFO] Frame count:", frame_count)

    # 发送线程
    def sender():
        nonlocal send_count

        try:
            for index in range(frame_count):

                length = min(
                    FRAME_SIZE,
                    TOTAL_SIZE - index * FRAME_SIZE
                )

                payload = make_payload(index, length)
                packet = make_packet(payload)

                sock.sendall(packet)

                send_hash.update(payload)
                send_count += len(payload)

                # 控制发送节奏
                time.sleep(0.001)

            print("[SENDER] Completed successfully")

        except Exception as e:
            send_errors.append(e)
            print("[SENDER ERROR]", repr(e))

    thread = threading.Thread(target=sender)
    thread.start()

    try:
        # 主线程持续接收
        for index in range(frame_count):

            header = recv_exact(sock, 8)

            magic, version, msg_type, length = struct.unpack(
                "!HBBI", header
            )

            assert magic == MAGIC
            assert version == VERSION
            assert msg_type == MSG_TYPE

            expected_length = min(
                FRAME_SIZE,
                TOTAL_SIZE - index * FRAME_SIZE
            )

            assert length == expected_length

            payload = recv_exact(sock, length)

            expected_payload = make_payload(index, length)

            assert payload == expected_payload

            recv_hash.update(payload)
            recv_count += len(payload)

    except Exception as e:
        print("[RECEIVER ERROR]", repr(e))

        # 接收失败时通知发送线程停止
        try:
            sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass

        raise

    finally:
        thread.join()
        sock.close()

    if send_errors:
        raise send_errors[0]

    print("[SEND] Bytes:", send_count)
    print("[RECV] Bytes:", recv_count)

    print("[SEND] SHA256:", send_hash.hexdigest())
    print("[RECV] SHA256:", recv_hash.hexdigest())

    assert send_count == TOTAL_SIZE
    assert recv_count == TOTAL_SIZE
    assert send_hash.digest() == recv_hash.digest()

    print("[PASS] TCP 1MB protocol test")


if __name__ == "__main__":
    main()

