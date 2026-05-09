# Test executables
add_executable(test_btree test_btree.cpp)
target_link_libraries(test_btree cabtree)
add_test(NAME btree_test COMMAND test_btree)

add_executable(test_monitor test_monitor.cpp)
target_link_libraries(test_monitor cabtree)
add_test(NAME monitor_test COMMAND test_monitor)

add_executable(test_adaptive test_adaptive.cpp)
target_link_libraries(test_adaptive cabtree)
add_test(NAME adaptive_test COMMAND test_adaptive)
