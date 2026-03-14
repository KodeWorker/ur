include(GoogleTest)

# Adds a GoogleTest-based test executable and registers it for CTest. Usage:
# ur_test(<test_name>) Expects a <test_name>.cpp file in the current directory.
function(ur_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE ur_lib GTest::gtest_main)
  gtest_discover_tests(${name})
endfunction()

ur_test(test_workspace)
ur_test(test_database)
ur_test(test_crypto)
ur_test(test_http_provider)
ur_test(test_runner)
ur_test(test_chat)
ur_test(test_persona_updater)
ur_test(test_sandbox)
ur_test(test_loader)
ur_test(test_executor)
ur_test(test_builtin_tools)
