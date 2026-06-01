# import socket
# import json
# import pyautogui

# # It's necessary to get real-time changes on screen as packets arrive
# pyautogui.PAUSE = 0.0
# # The program wouldn't crash if for example mouse hits edges of the screen
# pyautogui.FAILSAFE = False

# # server listens to all network interfaces including wi-fi
# HOST = '0.0.0.0'
# PORT = 5000

# def start_server():
#     server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

#     # By default TCP uses Nagel algorithm to reduce network traffic
#     # It simply gathers small packets and send them together  
#     # In order to have a real-time server we'd turn this algorithm off by socket.TCP_NODELAY 
#     server_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    
#     try:
#         server_socket.bind((HOST, PORT))
#         server_socket.listen(1)
#         print(f"[*] Server is listening on port {PORT}...")
#         print("[*] Please connect your Android device or simulator.")
        
#         # Accept a conenction from client
#         client_socket, client_address = server_socket.accept()
#         print(f"[+] Connected to device: {client_address}")
        
#         #Activate no delay for client as well
#         client_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        
#         buffer = ""
#         while True:
#             # 4096 is buffer size. It's big enough for our small packets
#             data = client_socket.recv(4096).decode('utf-8')
#             if not data: # If a connection is made with no data, means the device is disconnected
#                 print("[-] Device disconnected.")
#                 break
                
#             buffer += data
#             # The is a chance some packets arrive at the same time
#             # The socket buffers them so we'll just split them by \n as process them sequentially 
#             while "\n" in buffer:
#                 line, buffer = buffer.split("\n", 1)
#                 if not line.strip():
#                     continue
                
#                 try:
#                     # Client serializes its json data to send it here, so we'll just deserialize it into a python dictionary
#                     packet = json.loads(line)
#                     print("New packet: " + line)
#                     packet_type = packet.get("type")
                    
#                     if packet_type == "move":
#                         dx = packet.get("dx", 0)
#                         dy = packet.get("dy", 0)
#                         pyautogui.moveRel(dx, dy)
                        
#                     elif packet_type == "click":
#                         button = packet.get("button", "left")
#                         pyautogui.click(button=button)
                        
#                         # We don't want to drop any click packets
#                         ack_packet = {"status": "ACK", "type": "click", "timestamp": packet.get("timestamp")}
#                         client_socket.sendall((json.dumps(ack_packet) + "\n").encode('utf-8'))
#                         print(f"[#] Click ACK sent for timestamp: {packet.get('timestamp')}")
                        
#                     elif packet_type == "scroll":
#                         direction = packet.get("scroll_dir", 0)
#                         pyautogui.scroll(direction * 100)

#                         # We don't want to drop any click packets
#                         ack_packet = {"status": "ACK", "type": "scroll", "timestamp": packet.get("timestamp")}
#                         client_socket.sendall((json.dumps(ack_packet) + "\n").encode('utf-8'))
#                         print(f"[#] Scroll ACK sent for timestamp: {packet.get('timestamp')}")

                        
#                 except json.JSONDecodeError:
#                     print(f"[!] Error decoding JSON: {line}")
#                 except Exception as e:
#                     print(f"[!] Error processing command: {e}")
                    
#     except Exception as e:
#         print(f"[!] Server error: {e}")
#     finally:
#         server_socket.close()
#         print("[*] Server stopped.")

# if __name__ == "__main__":
#     start_server()

import socket
import json
import pyautogui

# برای اعمال تغییرات آنی پکت‌های سنسور روی اسکرین لپ‌تاپ
pyautogui.PAUSE = 0.0
# جلوگیری از کرش کردن برنامه در صورت برخورد موس به لبه‌های مانیتور
pyautogui.FAILSAFE = False

# سرور روی تمام اینترفیس‌های شبکه (از جمله وای‌فای) گوش می‌دهد
HOST = '0.0.0.0'
PORT = 5000

def start_server():
    # ۱. ساخت سوکت UDP با استفاده از SOCK_DGRAM
    # در UDP الگوریتم Nagle وجود ندارد، پس نیازی به تنظیم TCP_NODELAY نیست.
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    try:
        # ۲. بایند کردن پورت
        server_socket.bind((HOST, PORT))
        print(f"[*] UDP Server is listening on port {PORT}...")
        print("[*] Please send UDP packets from your Android device.")
        
        while True:
            try:
                # ۳. دریافت بسته مستقل (Datagram) از شبکه
                # recvfrom برخلاف recv، آدرس فرستنده (آی‌پي و پورت گوشی) را هم برمی‌گرداند
                data, client_address = server_socket.recvfrom(4096)
                
                if not data:
                    continue
                
                line = data.decode('utf-8').strip()
                if not line:
                    continue
                
                try:
                    # دی‌سریالایز کردن دیتای جی‌سون دریافتی
                    packet = json.loads(line)
                    print(f"New packet from {client_address}: {line}")
                    packet_type = packet.get("type")
                    
                    # الف) پردازش دیتای حرکت موس هوایی (با فرکانس بالا ارسال می‌شود)
                    if packet_type == "move":
                        dx = packet.get("dx", 0)
                        dy = packet.get("dy", 0)
                        pyautogui.moveRel(dx, dy)
                        
                    # ب) پردازش کلیک (حساس به گم شدن پکت)
                    elif packet_type == "click":
                        button = packet.get("button", "left")
                        pyautogui.click(button=button)
                        
                        # ارسال ACK بازگشتی در UDP با استفاده از sendto به آدرس فرستنده
                        ack_packet = {"status": "ACK", "type": "click", "timestamp": packet.get("timestamp")}
                        server_socket.sendto((json.dumps(ack_packet) + "\n").encode('utf-8'), client_address)
                        print(f"[#] Click ACK sent via UDP to {client_address} for timestamp: {packet.get('timestamp')}")
                        
                    # ج) پردازش اسکرول
                    elif packet_type == "scroll":
                        direction = packet.get("scroll_dir", 0)
                        pyautogui.scroll(direction * 100)

                        # ارسال ACK بازگشتی برای اسکرول
                        ack_packet = {"status": "ACK", "type": "scroll", "timestamp": packet.get("timestamp")}
                        server_socket.sendto((json.dumps(ack_packet) + "\n").encode('utf-8'), client_address)
                        print(f"[#] Scroll ACK sent via UDP to {client_address} for timestamp: {packet.get('timestamp')}")
                        
                except json.JSONDecodeError:
                    print(f"[!] Error decoding JSON: {line}")
                except Exception as e:
                    print(f"[!] Error processing command: {e}")
                    
            except Exception as e:
                print(f"[!] Error receiving data: {e}")
                
    except Exception as e:
        print(f"[!] Server error: {e}")
    finally:
        server_socket.close()
        print("[*] Server stopped.")

if __name__ == "__main__":
    start_server()