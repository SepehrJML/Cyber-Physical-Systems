import socket
import json
import time

SERVER_IP = '127.0.0.1'  # چون روی همین لپ‌تاپ تست می‌کنیم
PORT = 5000

def mock_move(client_socket):
    move_packet = {
        "type": "move",
        "dx": 5,   # حرکت ۵ پیکسل به راست
        "dy": 2,   # حرکت ۲ پیکسل به پایین
        "timestamp": int(time.time() * 1000)
    }
    client_socket.sendall((json.dumps(move_packet) + "\n").encode('utf-8'))
    time.sleep(0.02)  # شبیه‌سازی نرخ ۵۰ هرتز سنسور

def mock_click(client_socket):
    # ۲. تست کلیک و دریافت ACK از سرور
    print("Testing Click with ACK...")
    click_packet = {
        "type": "click",
        "button": "left",
        "timestamp": int(time.time() * 1000)
    }
    client_socket.sendall((json.dumps(click_packet) + "\n").encode('utf-8'))
    
    # انتظار برای دریافت تاییدیه (ACK) از سرور
    client_socket.settimeout(2.0)  # ۲ ثانیه تایم‌اوت
    response = client_socket.recv(1024).decode('utf-8')
    print(f"Received from server: {response.strip()}")


def run_mock_client():
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    
    try:
        print(f"Connecting to {SERVER_IP}:{PORT}...")
        client_socket.connect((SERVER_IP, PORT))
        print("Connected!")


        time.sleep(1)
        mock_click(client_socket)
        time.sleep(1)
        mock_click(client_socket)

        print("Testing Mouse Movement...")
        for i in range(200):
            mock_move(client_socket)

        time.sleep(1)
        mock_click(client_socket)
        time.sleep(1)
        mock_click(client_socket)
        time.sleep(1)
        mock_click(client_socket)


    except Exception as e:
        print(f"Error: {e}")
    finally:
        client_socket.close()

if __name__ == "__main__":
    run_mock_client()