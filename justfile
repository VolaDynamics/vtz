
clean:
    rm -rf build install

# NOTES
#
# We set VTZ_BUILD_SHARED to be ON here because we want to ensure that
# symbol visibility is correct when building/testing.
gen config="Release" *extra:
    cmake \
        -B build \
        -G Ninja \
        -DVTZ_BUILD_SHARED=ON \
        -DCMAKE_BUILD_TYPE={{config}} \
        -DCMAKE_INSTALL_PREFIX=install {{extra}}

build config="Release" *extra:
    @just gen {{config}} {{extra}}
    cmake --build build


