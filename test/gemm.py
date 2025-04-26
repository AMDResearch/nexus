#!/usr/bin/env python3

import torch

# Ensure we're using ROCm and the GPU
assert torch.version.hip is not None, "This script requires ROCm."
device = torch.device("cuda")

# Define matrix sizes
M, K, N = 512, 256, 512

# Initialize matrices A and B
A = torch.randn(M, K, device=device)
B = torch.randn(K, N, device=device)

# Perform matrix multiplication: C = A @ B
C = A @ B

# Optional: verify on CPU
# A_cpu = A.cpu()
# B_cpu = B.cpu()
# C_ref = A_cpu @ B_cpu
# assert torch.allclose(C.cpu(), C_ref, atol=1e-5)

print("GEMM (matrix multiplication) completed successfully on ROCm GPU.")
