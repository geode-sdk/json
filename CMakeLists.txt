cmake_minimum_required(VERSION 3.21)

project(mat-json VERSION 3.0.0)

set(SOURCE_FILES
    src/external/dragonbox.cpp
    src/value.cpp
    src/impl.cpp
    src/parser.cpp
    src/dump.cpp
)

if (NOT COMMAND CPMAddPackage)
    include(cmake/get_cpm.cmake)
endif()

CPMAddPackage("gh:geode-sdk/result#58c15d8")

# This option is only useful for Geode itself
if (DEFINED MAT_JSON_AS_INTERFACE AND MAT_JSON_AS_INTERFACE)
    add_library(mat-json INTERFACE)
    target_include_directories(mat-json INTERFACE include)

    add_library(mat-json-impl INTERFACE)
    target_link_libraries(mat-json-impl INTERFACE mat-json)
    target_sources(mat-json-impl INTERFACE ${SOURCE_FILES})
else()
    add_library(mat-json ${SOURCE_FILES})

    target_compile_features(mat-json PUBLIC cxx_std_20)

    target_include_directories(mat-json PUBLIC include)
    target_link_libraries(mat-json PUBLIC GeodeResult)
endif()

if (PROJECT_IS_TOP_LEVEL)
    target_compile_options(mat-json PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
    )
    add_subdirectory(test)
endif()
