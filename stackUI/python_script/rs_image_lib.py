# python 3
from unireedsolomon import rs
from math import ceil, log
import numpy as np
import struct, bitarray

IMAGE = "timg.jpg"
RS_FILE = "timg.jpg.rs"
RE_CONTRUCT_IMAGE = "timg_recontruct.jpg"
PACKET_SIZE = 980

def add_n_k_to_image(image_file, code_word_num):
    with open(image_file, 'rb') as f:
        image_contain = f.read()
    k = ceil(len(image_contain) / float(PACKET_SIZE))
    #k = 9
    n = 2 ** ceil(log(k) / log(2))
    byte_n = bitarray.bitarray(format(int(n), "016b")).tobytes()
    byte_k = bitarray.bitarray(format(int(k), "016b")).tobytes()
    fout = open('no_change', 'wb')
    fout.write(image_contain )
    fout.close()
    image_contain = byte_n + byte_k + image_contain 
    fout = open('change', 'wb')
    fout.write(image_contain )
    fout.close()
    return image_contain 
    
    
    

    
def rs_encode_image(image_file, code_word_num):
    with open(image_file, 'rb') as f:
        image_contain = f.read()
    image_contain = add_n_k_to_image(image_file, code_word_num)
    k = ceil(len(image_contain) / float(PACKET_SIZE))
    #k = 9
    n = 2 ** ceil(log(k) / log(2))
    #n = 16
    print(type(image_contain))
    print(k, n)
    fout = open(image_file+'.rs', 'wb')
    mat = []
    for i in range(k):
        if(len(image_contain) > (i+1) * PACKET_SIZE):
            mat_temp = list(image_contain[i * PACKET_SIZE : (i+1) * PACKET_SIZE])
            mat.append(mat_temp)
        else:
            empty = [0 for ii in range(PACKET_SIZE)]
            empty[:(len(image_contain) - i * PACKET_SIZE)] = \
                list(image_contain[i * PACKET_SIZE : len(image_contain)])
            mat.append(empty)
    for i in range(n-k):
        mat.append([0 for ii in range(PACKET_SIZE)])

    mat_arr_orig = np.array(mat)
    mat_arr_code = mat_arr_orig

    coder = rs.RSCoder(n, k)
    for i in range(PACKET_SIZE):
        #  source = bytes([int(ii) for ii in mat_arr_orig[:k, i]])
        source = ''.join(chr(ii) for ii in mat_arr_orig[:k, i])
        code_word = list(coder.encode(source))
        mat_arr_code[:, i] = [ord(ii) for ii in code_word]

    out_image_contain = b''
    for line in mat_arr_code:
        out_bytes = b''.join([struct.pack("B", ii) for ii in list(line)])
        out_image_contain += out_bytes
        fout.write(out_bytes)
    fout.close()           

def rs_decode_image(rs_file, code_word_num, n, k):
    with open(rs_file, 'rb') as f:
        rs_contain = f.read()
    
   # k = 9
   # n = 16
    n = int(n)
    k = int(k)
    fout = open(rs_file+'.re', 'wb')

    mat = []
    for i in range(n):
        if(len(rs_contain) >= (i+1) * PACKET_SIZE):
            mat_temp = list(rs_contain[i * PACKET_SIZE : (i+1) * PACKET_SIZE])
            mat.append(mat_temp)
        else:
            empty = [0 for ii in range(PACKET_SIZE)]
            if len(rs_contain) > (i+1) *PACKET_SIZE:
                empty[:(len(rs_contain) - i * PACKET_SIZE)] = \
                        list(rs_contain[i * PACKET_SIZE : len(rs_contain)])
            mat.append(empty)
    mat_arr_code = np.array(mat)            
    mat_arr_orig = mat_arr_code[:k, :]

    coder = rs.RSCoder(n, k)
    for i in range(PACKET_SIZE):
        code_word = b''.join(struct.pack("B", ii) for ii in mat_arr_code[:, i]) # bytes
        source = coder.decode(code_word)[0] # list str
        source = [ii for ii in source]

        if len(source) < k:
            mat_arr_orig[0:(k-len(source)), i] = [0 for i in range(k-len(source))]
            mat_arr_orig[(k-len(source)):, i] = [ord(ii) for ii in source]
        else:
            mat_arr_orig[:, i] = [ord(ii) for ii in source]

    out_image_contain = b''
    
    out_bytes = b''.join([struct.pack("B", ii) for ii in mat_arr_orig[0, 4:]])
    fout.write(out_bytes)
    for line in mat_arr_orig[1:, :]:
        out_bytes = b''.join([struct.pack("B", ii) for ii in list(line)])
        out_image_contain += out_bytes
        fout.write(out_bytes)
    fout.close()        
    return mat_arr_code, mat_arr_orig, rs_contain, out_image_contain




if __name__ == "__main__":
    pass
    #  mat_arr_orig, image_contain, mat, mat_arr_code, source, code_word = rs_encode_image(IMAGE, 1)
    #  mat_arr_code, mat_arr_orig, rs_contain, out_image_contain = rs_decode_image(RS_FILE, 1)
    
   # add_n_k_to_image(IMAGE, 1)
    
    mat_arr_code, mat_arr_orig, rs_contain, out_image_contain =  rs_decode_image('193', 1, 16, 9)
