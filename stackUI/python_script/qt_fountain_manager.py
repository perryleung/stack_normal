import sys
sys.path.append('.')
from node_lib import qt_fountain_manager
if __name__ == "__main__":
    print(sys.argv)
    qt_fountain_manager(sys.argv)
