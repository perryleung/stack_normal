import sys
sys.path.append('.')
from rs_image_lib import rs_encode_image
if __name__ == "__main__":
    rs_encode_image(sys.argv[1], 1)
