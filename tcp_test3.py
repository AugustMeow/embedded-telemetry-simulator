import socket
import hashlib
import time

HOST = "127.0.0.1"
PORT = 10000
DATA_SIZE = 10* 1024 * 1024  # 1MB

# 生成确定性的测试数据：0~255 循环
send_data = bytes(range(256)) * (DATA_SIZE // 256)

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.settimeout(30)

try:
    client.connect((HOST, PORT))
    print("[INFO] Connected to server")

    # 发送 1MB
    client.sendall(send_data)
    print(f"[INFO] Sent: {len(send_data)} bytes")

    # 接收完整回显
    recv_data = bytearray()

    while len(recv_data) < len(send_data):
        chunk = client.recv(4096)

        if not chunk:
            print("[ERROR] Server disconnected")
            break

        recv_data.extend(chunk)

    print(f"[INFO] Received: {len(recv_data)} bytes")

    # 比较数据
    if bytes(recv_data) == send_data:
        print("[PASS] Data integrity verified")
    else:
        print("[FAIL] Data mismatch")

    print("Send SHA256:", hashlib.sha256(send_data).hexdigest())
    print("Recv SHA256:", hashlib.sha256(recv_data).hexdigest())

except socket.timeout:
    print("[ERROR] Receive timeout")

except OSError as e:
    print("[ERROR]", e)

finally:
    client.close()
