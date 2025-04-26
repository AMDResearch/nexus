kernel="vector_add_thrust"
# kernel="vector_add_template"
kernel="vector_add_inline"
# kernel="vector_add"

kernel="gemm"
output="$kernel.json"

echo "output: $output"

export HSA_TOOLS_LIB=./build/lib/libnexus.so
export NEXUS_OUTPUT_FILE=$output
export NEXUS_LOG_LEVEL=0

binary="python ./test/$kernel.py"
# binary=./build/test/$kernel
# binary=./test/vector_add_template


$binary


echo "Nexus output file: $output"
