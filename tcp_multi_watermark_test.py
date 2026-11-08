
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

# A：慢客户端，1MB
A_TOTAL_SIZE = 1 * 1024 * 1024

# B：正常客户端，256KB
B_TOTAL_SIZE = 256 * 1024


def make_payload(index, length, seed):
    return bytes((seed + index + j) % 256 for j in range(length))


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
                f"Server disconnected: expected={size}, "
                f"received={len(data)}"
            )

        data.extend(chunk)

    return bytes(data)


def send_frames(sock, total_size, seed):
    frame_count = (total_size + FRAME_SIZE - 1) // FRAME_SIZE
    digest = hashlib.sha256()
    sent = 0

    for index in range(frame_count):
        length = min(FRAME_SIZE, total_size - sent)

        payload = make_payload(index, length, seed)
        sock.sendall(make_packet(payload))

        digest.update(payload)
        sent += length

    return sent, digest.hexdigest()


def recv_frames(sock, total_size, seed):
    frame_count = (total_size + FRAME_SIZE - 1) // FRAME_SIZE
    digest = hashlib.sha256()
    received = 0

    for index in range(frame_count):
        header = recv_exact(sock, 8)

        magic, version, msg_type, length = struct.unpack(
            "!HBBI", header
        )

        assert magic == MAGIC, f"Invalid magic: {magic:#x}"
        assert version == VERSION
        assert msg_type == MSG_TYPE

        expected_length = min(
            FRAME_SIZE,
            total_size - received
        )

        assert length == expected_length

        payload = recv_exact(sock, length)

        expected = make_payload(index, length, seed)
        assert payload == expected, f"Payload mismatch: frame {index}"

        digest.update(payload)
        received += length

    return received, digest.hexdigest()

def test_normal_client(sock, total_size, seed):
    frame_count = (total_size + FRAME_SIZE - 1) // FRAME_SIZE

    send_hash = hashlib.sha256()
    recv_hash = hashlib.sha256()

    sent = 0
    received = 0

    for index in range(frame_count):
        length = min(FRAME_SIZE, total_size - sent)

        payload = make_payload(index, length, seed)

        # 发送一帧
        sock.sendall(make_packet(payload))
        send_hash.update(payload)
        sent += length

        # 立即接收对应响应
        header = recv_exact(sock, 8)

        magic, version, msg_type, resp_length = struct.unpack(
            "!HBBI", header
        )

        assert magic == MAGIC
        assert version == VERSION
        assert msg_type == MSG_TYPE
        assert resp_length == length

        response = recv_exact(sock, resp_length)

        assert response == payload

        recv_hash.update(response)
        received += len(response)

    return sent, received, send_hash.hexdigest(), recv_hash.hexdigest()

def main():

    a = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    b = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    a.settimeout(60)
    b.settimeout(30)

    a.connect((HOST, PORT))
    print("[A] Connected: slow client")

    a_errors = []
    a_send_result = {}

    # A 的发送线程
    def a_sender():
        try:
            sent, digest = send_frames(
                a, A_TOTAL_SIZE, 10
            )

            a_send_result["bytes"] = sent
            a_send_result["hash"] = digest

            print("[A] Send completed:", sent)

        except Exception as e:
            a_errors.append(e)
            print("[A] SEND ERROR:", repr(e))

    a_thread = threading.Thread(target=a_sender)
    a_thread.start()

    # A 暂时不读取服务端响应
    print("[A] Pause receiving for 2 seconds")
    time.sleep(2)

    # 此时连接 B
    b.connect((HOST, PORT))
    print("[B] Connected: normal client")

    b_sent, b_received, b_send_hash, b_recv_hash = test_normal_client(
    b, B_TOTAL_SIZE, 100)

    assert b_sent == B_TOTAL_SIZE
    assert b_received == B_TOTAL_SIZE
    assert b_send_hash == b_recv_hash

    print("[B] Sent:", b_sent)
    print("[B] Received:", b_received)
    print("[B] SHA256:", b_recv_hash)
    print("[B] PASS - normal client unaffected")

    # B 测试结束后，A 开始恢复读取
    
    print("[A] Start receiving")

    a_received, a_recv_hash = recv_frames(
        a, A_TOTAL_SIZE, 10
    )

    # 等待 A 发送线程结束
    a_thread.join()

    if a_errors:
        raise a_errors[0]

    a_sent = a_send_result["bytes"]
    a_send_hash = a_send_result["hash"]

    assert a_sent == A_TOTAL_SIZE
    assert a_received == A_TOTAL_SIZE
    assert a_send_hash == a_recv_hash

    print("[A] Sent:", a_sent)
    print("[A] Received:", a_received)
    print("[A] SHA256:", a_recv_hash)
    print("[A] PASS - slow client recovered")

    a.close()
    b.close()

    print("\n[PASS] Multi-client watermark isolation test")


if __name__ == "__main__":
    main()

