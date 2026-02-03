
clean:
    rm -rf build install

gen config="Release" *extra:
    cmake \
        -B build \
        -G Ninja \
        -DCMAKE_BUILD_TYPE={{config}} \
        -DCMAKE_INSTALL_PREFIX=install {{extra}}

build config="Release" *extra:
    @just gen {{config}} {{extra}}
    cmake --build build


