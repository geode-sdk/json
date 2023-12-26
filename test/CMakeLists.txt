Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.0
)

FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

set(CMAKE_CXX_STANDARD 20)
add_executable(tests test.cpp)
target_link_libraries(tests PRIVATE mat-json Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(tests)