#!/usr/bin/env python3

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
