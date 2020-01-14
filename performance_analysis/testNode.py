import socket
import sys

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(('127.0.0.1', 16888))
print('port ', sys.argv[1])
s.send(sys.argv[1])
s.close()
