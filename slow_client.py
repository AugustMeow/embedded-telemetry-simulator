import socket
import time

HOST = "127.0.0.1"
PORT = 10000

CHUNK_SIZE = 4096
MAX_SEND = 32 * 1024 * 1024

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
client.settimeout(30)

total_sent = 0

try:
    client.connect((HOST, PORT))
    print("[INFO] Connected")

    # 等待正常客户端启动
    print("[INFO] Waiting 10 seconds...")
    time.sleep(10)

    print("[INFO] Start sending")

    data = b'A' * CHUNK_SIZE

    while total_sent < MAX_SEND:
        client.sendall(data)
        total_sent += len(data)

        if total_sent % (1024 * 1024) == 0:
            print(f"[INFO] Sent: {total_sent // 1024 // 1024} MB")

    print("[INFO] Finished sending")

except (BrokenPipeError, ConnectionResetError) as e:
    print("[INFO] Server closed connection:", e)

except socket.timeout:
    print("[INFO] Send timeout")

except OSError as e:
    print("[ERROR]", e)

finally:
    print("[INFO] Total attempted sent:", total_sent)
    client.close()
