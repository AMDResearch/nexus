
#include <thrust/device_vector.h>
#include <thrust/sequence.h>
#include <cstdint>
#include <iostream>

__global__ void reduction_kernel(double* input, double* result, std::size_t count) {
  const auto thread_id = threadIdx.x + blockIdx.x * blockDim.x;
  if (thread_id < count) {
    const auto value = input[thread_id];
    atomicAdd(result, value / (thread_id + 1));
  }
}

int main() {
  const std::size_t count{10000000};
  thrust::device_vector<double> input(count, 1);
  thrust::device_vector<double> result(1, 0);
  thrust::sequence(input.begin(), input.end(), 1);

  const std::size_t block_size{512};
  const std::size_t num_blocks{(count + block_size - 1) / block_size};

  std::cout << "input: " << input.data().get() << std::endl;
  std::cout << "result: " << result.data().get() << std::endl;

  reduction_kernel<<<num_blocks, block_size, block_size * sizeof(double)>>>(
      input.data().get(), result.data().get(), count);

  const auto status = hipDeviceSynchronize();
  if (status != hipSuccess || result[0] != count) {
    std::cout << "Kernel failed.\n";
    return -1;
  } else {
    std::cout << "Success!";
  }
  std::cout << std::endl;
  return static_cast<int>(result[0]);
}
