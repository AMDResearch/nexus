
import logging
import time
import os
import numpy as np
import ctypes
import stat
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from hip import memcpy_d2h, open_ipc_handle

def read_ipc_handles(args, ipc_file_name):
    count = sum(1 for arg in args if "*" in arg and "const" not in arg)
    
    handles = []
    sizes = []
    handles_set = set()

    while len(handles) < count:
        if not os.path.exists(ipc_file_name):
            logging.debug("Waiting for IPC file...")
            time.sleep(0.1)
            continue

        with open(ipc_file_name, "rb") as file:
            data = file.read()

        messages = data.split(b"BEGIN\n")
        for message in messages:
            if b"END\n" in message:
                content = message.split(b"END\n")[0]

                if len(content) == 72:
                    handle_data = content[:64]
                    size_data = content[64:72]

                    handle_np = np.frombuffer(handle_data, dtype=np.uint8)
                    handle_tuple = tuple(handle_np)

                    if handle_tuple not in handles_set:
                        handles.append(handle_np)
                        handles_set.add(handle_tuple)

                        size_value = int.from_bytes(size_data, byteorder="little")
                        sizes.append(size_value)

                        logging.debug("Final IPC Handle (hex):")
                        for i in range(0, len(handle_np), 16):
                            chunk = handle_np[i : i + 16]
                            logging.debug(" ".join(f"{b:02x}" for b in chunk))

                        logging.debug(f"Corresponding Pointer Size: {size_value} bytes")

        if len(handles) < count:
            logging.debug(f"Waiting for {count - len(handles)} more IPC handles...")
            time.sleep(0.1)

    logging.debug(f"Successfully read {len(handles)} IPC handles and sizes.")
    return handles, sizes



def send_response(pipe_name):
    with open(pipe_name, "w") as fifo:
        fifo.write("done\n")

def get_kern_arg_data(pipe_name, args, ipc_file_name):
    if not os.path.exists(pipe_name):
        os.mkfifo(pipe_name)
        os.chmod(pipe_name, stat.S_IRWXU | stat.S_IRWXG | stat.S_IRWXO)

    with open(pipe_name, "rb") as fifo:
        ipc_handles, ptr_sizes = read_ipc_handles(args, ipc_file_name)

    type_map = {
        "double*": ctypes.c_double,
        "float*": ctypes.c_float,
        "int*": ctypes.c_int,
        "std::size_t*": ctypes.c_size_t,
    }
    results = []
    pointer_args = list(filter(lambda arg: "*" in arg and "const" not in arg, args))

    for handle, arg, array_size in zip(ipc_handles, pointer_args, ptr_sizes):
        ptr = open_ipc_handle(handle)
        logging.debug(f"Opened IPC Ptr: {ptr} (0x{ptr:x})")
        arg_type = arg.split()[0]
        if arg_type in type_map:
            dtype = type_map[arg_type]
        else:
            raise TypeError(f"Unsupported pointer type: {arg_type}")
        num_elements = array_size // ctypes.sizeof(dtype)
        # max_num_elements = 32
        num_elements_to_copy = num_elements
        host_array = memcpy_d2h(ptr, num_elements_to_copy, dtype)
        logging.debug(
            f"Received data from IPC ({arg_type}/{num_elements}): {host_array}"
        )
        results.append(host_array)
    return results
