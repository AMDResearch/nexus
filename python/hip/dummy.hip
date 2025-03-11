#include <hip/hip_runtime.h>

#include <iostream>

__global__ void emptyKernel(int* value) {
  // No operation, just receiving the argument
}

int main() {
  int* arg;  // Example argument
  hipMalloc(&arg, sizeof(int));

  dim3 grid(1);
  dim3 block(1);

  hipLaunchKernelGGL(emptyKernel, grid, block, 0, 0, arg);

  hipDeviceSynchronize();

  std::cout << "Empty kernel executed successfully with argument: " << arg
            << std::endl;
  return 0;
}
