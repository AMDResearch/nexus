#include <hip/hip_runtime.h>
#include <thrust/device_vector.h>
#include <thrust/functional.h>
#include <thrust/host_vector.h>
#include <thrust/transform.h>
#include <iostream>

int main() {
  static constexpr std::size_t num_elements = 1024;

  thrust::device_vector<float> d_a(num_elements, 1.0f);
  thrust::device_vector<float> d_b(num_elements, 2.0f);
  thrust::device_vector<float> d_c(num_elements);

  thrust::transform(
      d_a.begin(), d_a.end(), d_b.begin(), d_c.begin(), thrust::plus<float>());

  thrust::host_vector<float> h_c(d_c);

  bool success = true;
  for (std::size_t i = 0; i < num_elements; ++i) {
    if (h_c[i] != 3.0f) {
      success = false;
      break;
    }
  }

  std::cout << (success ? "Success" : "Failure") << std::endl;
  return success ? 0 : 1;
}
