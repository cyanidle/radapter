set -eu

conan install .\
    --deployer=runtime_deploy --deployer-folder=build/Release/bin \
    --build=missing \
    -o "libpq/*:shared=True" \
    -o '&:jit=static'
cmake --preset conan-release
ninja -C build/Release