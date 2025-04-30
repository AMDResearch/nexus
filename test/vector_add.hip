/****************************************************************************
 * MIT License
 * 
 * Copyright (c) 2025 Advanced Micro Devices, Inc. All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ****************************************************************************/

#include <hip/hip_runtime.h>
#include <thrust/device_vector.h>
#include <thrust/host_vector.h>
#include <iostream>

#define N 1024  // Number of elements

// HIP kernel for vector addition
__global__ void vector_add(const float* a, const float* b, float* c, int n) {
  int idx = threadIdx.x + blockIdx.x * blockDim.x;
  if (idx < n) {
    c[idx] = a[idx] + b[idx];
  }
}

int main() {
  // Host vectors
  thrust::host_vector<float> h_a(N, 1.0f);  // Initialize with 1.0
  thrust::host_vector<float> h_b(N, 2.0f);  // Initialize with 2.0
  thrust::host_vector<float> h_c(N);

  // Device vectors
  thrust::device_vector<float> d_a = h_a;
  thrust::device_vector<float> d_b = h_b;
  thrust::device_vector<float> d_c(N);

  // Launch kernel
  int threads_per_block = 256;
  int blocks_per_grid = (N + threads_per_block - 1) / threads_per_block;
  hipLaunchKernelGGL(vector_add,
                     dim3(blocks_per_grid),
                     dim3(threads_per_block),
                     0,
                     0,
                     thrust::raw_pointer_cast(d_a.data()),
                     thrust::raw_pointer_cast(d_b.data()),
                     thrust::raw_pointer_cast(d_c.data()),
                     N);

  // Copy result back to host
  h_c = d_c;

  // Verify result
  bool success = true;
  for (int i = 0; i < N; i++) {
    if (h_c[i] != 3.0f) {
      success = false;
      break;
    }
  }

  std::cout << (success ? "Success" : "Failure") << std::endl;
  return success ? 0 : 1;
}