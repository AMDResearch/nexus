kernel="vector_add"

export HSA_TOOLS_LIB=./build/lib/libnexus.so
export KERNEL_TO_TRACE=$kernel
export NEXUS_LOG_LEVEL=3

binary=./test/vector_add
# binary=./test/vector_add_template

gdb $binary

