#include <hip/hip_runtime.h>
#include <thrust/device_vector.h>
#include <thrust/host_vector.h>
#include <iostream>

#define N 1024  // Number of elements

// HIP kernel for vector addition
template <typename T>
__global__ void vector_add(const T* a, const T* b, T* c, int n) {
  int idx = threadIdx.x + blockIdx.x * blockDim.x;
  if (idx < n) {
    c[idx] = a[idx] + b[idx];
  }
}

int main() {
  // Host vectors
  using T = float;
  thrust::host_vector<T> h_a(N, 1.0f);  // Initialize with 1.0
  thrust::host_vector<T> h_b(N, 2.0f);  // Initialize with 2.0
  thrust::host_vector<T> h_c(N);

  // Device vectors
  thrust::device_vector<T> d_a = h_a;
  thrust::device_vector<T> d_b = h_b;
  thrust::device_vector<T> d_c(N);

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