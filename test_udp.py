import socket

UDP_IP = "192.168.1.248"
UDP_PORT = 4123
MESSAGE = '{"direction_offset": 10}\0'

print("UDP target IP:", UDP_IP)
print("UDP target port:", UDP_PORT)
print("message:", MESSAGE)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # UDP
sock.sendto(bytes(MESSAGE, "utf-8"), (UDP_IP, UDP_PORT))