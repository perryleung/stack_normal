import socket
import sys

def transmitPort(port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', 16888))
    print('port', port)
    s.send(port)
    print(" talk to ", port)
    s.close()

if __name__ == "__main__":
    transmitPort(sys.argv[1])
