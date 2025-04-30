################################################################################
# MIT License
# 
# Copyright (c) 2025 Advanced Micro Devices, Inc. All Rights Reserved.
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
################################################################################

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
