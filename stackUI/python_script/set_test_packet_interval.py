import sys
sys.path.append('.')
from node_lib import set_test_packet_interval
print(sys.argv[1])
set_test_packet_interval(sys.argv[1])
