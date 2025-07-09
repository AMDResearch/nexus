#! /usr/bin/env python3
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


import triton
import triton.language as tl
import torch

@triton.jit
def reduce(input_ptr, output_ptr, num_elements,
           BLOCK_SIZE: tl.constexpr):
    pid = tl.program_id(0)
    block_start = pid * BLOCK_SIZE

    offsets = block_start + tl.arange(0, BLOCK_SIZE)
    mask = offsets < num_elements

    vals = tl.load(input_ptr + offsets, mask=mask, other=0)
    acc = tl.sum(vals)

    tl.atomic_add(output_ptr + 0, acc)

def main():

    data_type = torch.int32
    BLOCK_SIZE = 128
    num_elements = 1_000_000
    grid = lambda META: (triton.cdiv(num_elements, META['BLOCK_SIZE']),)
    
    x = torch.randint(0, 42, (num_elements,), dtype=data_type, device='cuda')
    y = torch.zeros(1, dtype=data_type, device='cuda')

    reduce[grid](x, y, num_elements, BLOCK_SIZE=BLOCK_SIZE)

    actual = y.item()
    expected = int(x.cpu().sum())

    print(f"Expected: {expected:,} Actual: {actual:,}")

if __name__ == "__main__":
    main()