import sys
sys.path.append('.')
from node_lib import kill_a_node
if __name__ == "__main__":
    kill_a_node(int(sys.argv[1]))
