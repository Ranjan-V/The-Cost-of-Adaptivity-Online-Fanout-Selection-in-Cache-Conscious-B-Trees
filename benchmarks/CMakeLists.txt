# Benchmark executables
add_executable(bench_baseline benchmark_baseline.cpp)
target_link_libraries(bench_baseline cabtree)

add_executable(bench_adaptive benchmark_adaptive.cpp)
target_link_libraries(bench_adaptive cabtree)

add_executable(bench_ycsb benchmark_ycsb.cpp)
target_link_libraries(bench_ycsb cabtree)

add_executable(bench_threads benchmark_threads.cpp)
target_link_libraries(bench_threads cabtree)

add_executable(bench_dynamic benchmark_dynamic.cpp)
target_link_libraries(bench_dynamic cabtree)
