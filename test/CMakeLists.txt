Include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

CPMAddPackage("gh:fmtlib/fmt#12.1.0")

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/manual.cpp")
    add_executable(manual-testing manual.cpp)
    target_link_libraries(manual-testing PRIVATE mat-json fmt)
endif()

CPMAddPackage("gh:doctest/doctest#v2.5.3")
add_executable(tests test.cpp)
target_link_libraries(tests PRIVATE mat-json doctest::doctest)

include("${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")

doctest_discover_tests(tests)
