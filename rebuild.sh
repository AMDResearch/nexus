cmake -B build\
    -DCMAKE_PREFIX_PATH=${ROCM_PATH}\
    -DLLVM_INSTALL_DIR=/opt/rocm/llvm\
    -DCMAKE_BUILD_TYPE=Debug

cmake --build build --parallel 16

cd test && hipcc vector_add.hip -g -o vector_add --save-temps && cd ..
cd test && hipcc vector_add_template.hip -g -o vector_add_template --save-temps && cd ..