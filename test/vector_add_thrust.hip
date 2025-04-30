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
