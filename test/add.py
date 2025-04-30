#!/usr/bin/env python3
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


import torch

# Ensure we're using ROCm and the GPU
assert torch.version.hip is not None, "This script requires ROCm."
device = torch.device("cuda")

# Define vector size
N = 1024

# Initialize vectors A and B
A = torch.randn(N, device=device)
B = torch.randn(N, device=device)

# Perform vector addition: C = A + B
C = A + B

# Optional: verify on CPU
A_cpu = A.cpu()
B_cpu = B.cpu()
C_ref = A_cpu + B_cpu
assert torch.allclose(C.cpu(), C_ref, atol=1e-5)

print("Vector addition completed successfully on ROCm GPU.")
