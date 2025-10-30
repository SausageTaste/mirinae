set(gtest_libs GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)

add_executable(mirinae_test_custom_format custom_format.cpp)
add_test(NAME mirinae_test_custom_format COMMAND mirinae_test_custom_format)
target_link_libraries(mirinae_test_custom_format ${gtest_libs} mirinae::aux)
set_target_properties(mirinae_test_custom_format PROPERTIES
    FOLDER "mirinae/test"
)
