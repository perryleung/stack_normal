import sys
sys.path.append('.')
from node_lib import start_a_test_node
if __name__ == "__main__":
    print(sys.argv[1])
    start_a_test_node(sys.argv[1])
