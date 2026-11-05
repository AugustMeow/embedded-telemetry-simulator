
import socket
import hashlib
import threading

HOST = "127.0.0.1"
PORT = 10000
DATA_SIZE = 10 * 1024 * 1024  # 10 MiB

send_data = bytes(range(256)) * (DATA_SIZE // 256)

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.settimeout(30)

send_error = []

def sender():
    try:
        client.sendall(send_data)
        print(f"[SEND] Sent: {len(send_data)} bytes")

        # 告诉服务端：我已经发送完毕
        client.shutdown(socket.SHUT_WR)

    except OSError as e:
        send_error.append(e)
        print("[SEND ERROR]", e)

try:
    client.connect((HOST, PORT))
    print("[INFO] Connected to server")

    # 启动发送线程
    send_thread = threading.Thread(target=sender)
    send_thread.start()

    # 主线程持续接收回显
    recv_data = bytearray()

    while len(recv_data) < len(send_data):
        chunk = client.recv(4096)

        if not chunk:
            print("[ERROR] Server disconnected")
            break

        recv_data.extend(chunk)

    send_thread.join()

    print(f"[RECV] Received: {len(recv_data)} bytes")

    if bytes(recv_data) == send_data:
        print("[PASS] Data integrity verified")
    else:
        print("[FAIL] Data mismatch")

    print("Send SHA256:", hashlib.sha256(send_data).hexdigest())
    print("Recv SHA256:", hashlib.sha256(recv_data).hexdigest())

except socket.timeout:
    print("[ERROR] Socket timeout")

except OSError as e:
    print("[ERROR]", e)

finally:
    client.close()

