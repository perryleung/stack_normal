import sys
sys.path.append('.')
from node_lib import set_fsk1710_gain
if __name__ == "__main__":
    print(sys.argv[1])
    set_fsk1710_gain(sys.argv[1])
