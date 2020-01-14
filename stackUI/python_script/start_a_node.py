import sys
sys.path.append('.')
from node_lib import start_node
if __name__ == "__main__":
    print("from py start node :", sys.argv[1])
    start_node(int(sys.argv[1]))
