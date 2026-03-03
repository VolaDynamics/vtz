
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

docgen:
    uv run --python 3.11 etc/scripts/docgen.py

run_bench_groups:
    # Run format benchmarks
    build/bench_vtz --benchmark_filter='to_local_([a-z]+)$'
    build/bench_vtz --benchmark_filter='to_sys(_earliest|_latest)?_([a-z]+)$'
    build/bench_vtz --benchmark_filter='format'
    build/bench_vtz --benchmark_filter='parse'
    build/bench_vtz --benchmark_filter='locate_zone'
    build/bench_vtz --benchmark_filter='locate_random_zone'
