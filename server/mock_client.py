import socket
import json
import time

SERVER_IP = '127.0.0.1'  # چون روی همین لپ‌تاپ تست می‌کنیم
PORT = 5000
SERVER_ADDRESS = (SERVER_IP, PORT)

def mock_move(client_socket):
    move_packet = {
        "type": "move",
        "dx": 5,   # حرکت ۵ پیکسل به راست
        "dy": 2,   # حرکت ۲ پیکسل به پایین
        "timestamp": int(time.time() * 1000)
    }
    # در UDP به جای sendall از sendto استفاده می‌شود و آدرس مقصد مشخص می‌گردد
    # نیازی به اضافه کردن '\n' در انتهای رشته نیست
    client_socket.sendto(json.dumps(move_packet).encode('utf-8'), SERVER_ADDRESS)
    time.sleep(0.02)  # شبیه‌سازی نرخ ۵۰ هرتز سنسور گوشی

def mock_click(client_socket):
    # ۲. تست کلیک و دریافت ACK از سرور
    print("Testing Click with ACK via UDP...")
    click_packet = {
        "type": "click",
        "button": "left",
        "timestamp": int(time.time() * 1000)
    }
    client_socket.sendto(json.dumps(click_packet).encode('utf-8'), SERVER_ADDRESS)
    
    try:
        # انتظار برای دریافت تاییدیه (ACK) از سرور
        client_socket.settimeout(2.0)  # ۲ ثانیه تایم‌اوت برای جلوگیری از قفل شدن در صورت گم شدن پکت UDP
        
        # دریافت پاسخ و آدرس فرستنده (سرور)
        response, _ = client_socket.recvfrom(1024)
        print(f"Received from server: {response.decode('utf-8').strip()}")
    except socket.timeout:
        print("[!] Timeout: No ACK received from server (UDP packet might be dropped)")

def run_mock_client():
    # ۱. ساخت سوکت UDP با استفاده از SOCK_DGRAM
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    try:
        print(f"[*] Ready to send UDP packets to {SERVER_IP}:{PORT}...")

        time.sleep(1)
        mock_click(client_socket)

        print("Testing Mouse Movement...")
        for i in range(100):
            mock_move(client_socket)

        time.sleep(1)
        mock_click(client_socket)

    except Exception as e:
        print(f"Error: {e}")
    finally:
        client_socket.close()
        print("[*] Client stopped.")

if __name__ == "__main__":
    run_mock_client()