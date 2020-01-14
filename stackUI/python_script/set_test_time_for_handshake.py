import sys
sys.path.append('.')
from node_lib import set_test_time_for_handshake
if __name__ == "__main__":
    set_test_time_for_handshake(sys.argv[1])
