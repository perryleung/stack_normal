import sys
sys.path.append('.')
from node_lib import set_aloha_resend
if __name__ == "__main__":
    set_aloha_resend(sys.argv[1])
