import sys
sys.path.append('.')
from rs_image_lib import rs_decode_image
if __name__ == "__main__":
    rs_decode_image(sys.argv[1], 1, sys.argv[2], sys.argv[3])
