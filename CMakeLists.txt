cmake_minimum_required(VERSION 3.21)

project(mat-json VERSION 4.0.0)

set(SOURCE_FILES
    src/value.cpp
    src/impl.cpp
    src/parser.cpp
    src/dump.cpp
    src/iterator.cpp
)

if (NOT COMMAND CPMAddPackage)
    include(cmake/get_cpm.cmake)
endif()

if (NOT TARGET GeodeResult)
    CPMAddPackage("gh:geode-sdk/result@1.4.1")
endif()

if (MAT_JSON_AS_INTERFACE)
    if (MAT_JSON_BUILD_IMPL)
        set(_build_impl ON)
    else()
        set(_build_impl OFF)
    endif()
else()
    set(_build_impl ON)
endif()

if (_build_impl)
    CPMAddPackage("gh:geode-sdk/ordered-map#d4d347edfd26c50b9429522c9f112186d01a8698")
    CPMAddPackage("gh:fastfloat/fast_float@8.3.2")

    add_library(zmij src/external/zmij.cc)
    if (ANDROID_ABI STREQUAL "armeabi-v7a")
        # zmij fails to compile otherwise
        target_compile_definitions(zmij PRIVATE "ZMIJ_USE_SIMD=0")
    endif()
endif()

# This option is only useful for Geode itself
if (MAT_JSON_AS_INTERFACE)
    add_library(mat-json INTERFACE)
    target_include_directories(mat-json INTERFACE include)
    target_link_libraries(mat-json INTERFACE GeodeResult)

    if (_build_impl)
        add_library(mat-json-impl INTERFACE)
        target_link_libraries(mat-json-impl INTERFACE mat-json zmij tsl::ordered_map fast_float)
        target_sources(mat-json-impl INTERFACE ${SOURCE_FILES})
    endif()
else()
    add_library(mat-json ${SOURCE_FILES})

    target_compile_features(mat-json PUBLIC cxx_std_20)

    target_include_directories(mat-json PUBLIC include)
    target_link_libraries(mat-json PUBLIC GeodeResult)
    target_link_libraries(mat-json PRIVATE zmij tsl::ordered_map fast_float)
endif()

add_library(matjson ALIAS mat-json)

if (PROJECT_IS_TOP_LEVEL)
    if (NOT WIN32)
        add_compile_options("$<$<CONFIG:DEBUG>:-fsanitize=address,undefined>")
        add_link_options("$<$<CONFIG:DEBUG>:-fsanitize=address,undefined>")
    endif()
    target_compile_options(mat-json PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wno-gnu-statement-expression-from-macro-expansion>
    )
    add_subdirectory(test)
endif()
