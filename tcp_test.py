import socket
import hashlib
import threading
import time

HOST = "127.0.0.1"
PORT = 10000

DATA_SIZE = 10 * 1024 * 1024

send_data = bytes(range(256)) * (DATA_SIZE // 256)

recv_data = bytearray()
recv_error = []

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.settimeout(120)


def receiver():
    try:
        while len(recv_data) < DATA_SIZE:
            chunk = client.recv(4096)

            if not chunk:
                raise ConnectionError("Server disconnected")

            recv_data.extend(chunk)

    except Exception as e:
        recv_error.append(e)


try:

    # 等待正常客户端启动
    print("[INFO] Waiting 3 seconds...")
    #time.sleep(3)
    client.connect((HOST, PORT))
    print("[INFO] Connected")
    

    # 先启动接收线程
    thread = threading.Thread(target=receiver)
    thread.start()

    # 主线程发送
    client.sendall(send_data)
    print("[INFO] Send completed:", len(send_data))

    thread.join()

    if recv_error:
        raise recv_error[0]

    print("[INFO] Receive completed:", len(recv_data))

    send_hash = hashlib.sha256(send_data).hexdigest()
    recv_hash = hashlib.sha256(recv_data).hexdigest()

    print("Send SHA256:", send_hash)
    print("Recv SHA256:", recv_hash)

    if send_data == bytes(recv_data):
        print("[PASS] 10MB data integrity verified")
    else:
        print("[FAIL] Data mismatch")

except Exception as e:
    print("[ERROR]", repr(e))

finally:
    client.close()
