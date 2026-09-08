# Test tree. For commit 0.1 there is a single trivial smoke test so CTest has
# something to run; the Qt Test harness is wired up in commit 0.6.

add_executable(smoke_test smoke_test.cpp)
target_compile_features(smoke_test PRIVATE cxx_std_20)
add_test(NAME smoke COMMAND smoke_test)
