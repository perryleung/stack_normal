import sys
sys.path.append('.')
from node_lib import set_srt_entrys
if __name__ == "__main__":
    print(sys.argv[1], sys.argv[2])
    set_srt_entrys(sys.argv[1], sys.argv[2])
