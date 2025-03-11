#include <thrust/device_vector.h>
#include <thrust/sequence.h>
#include <cstdint>
#include <iostream>

__global__ void reduction_kernel(double* input, double* result, std::size_t count) {
  extern __shared__ double shared_data[];
  const auto thread_id = threadIdx.x + blockIdx.x * blockDim.x;
  const auto local_id = threadIdx.x;

  double sum = 0.0;
  if (thread_id < count) {
    sum = input[thread_id] / (thread_id + 1);
  }
  shared_data[local_id] = sum;
  __syncthreads();

  for (std::size_t stride = blockDim.x / 2; stride > 0; stride >>= 1) {
    if (local_id < stride) {
      shared_data[local_id] += shared_data[local_id + stride];
    }
    __syncthreads();
  }

  if (local_id == 0) {
    atomicAdd(result, shared_data[0]);
  }
}

int main() {
  const std::size_t count{10000000};
  thrust::device_vector<double> input(count, 1);
  thrust::device_vector<double> result(1, 0);
  thrust::sequence(input.begin(), input.end(), 1);

  const std::size_t block_size{512};
  const std::size_t num_blocks{(count + block_size - 1) / block_size};

  reduction_kernel<<<num_blocks, block_size, block_size * sizeof(double)>>>(
      input.data().get(), result.data().get(), count);

  const auto status = hipDeviceSynchronize();
  if (status != hipSuccess || result[0] != count) {
    std::cout << "Kernel failed.";
    return -1;
  } else {
    std::cout << "Success!";
  }
  std::cout << std::endl;
  return static_cast<int>(result[0]);
}