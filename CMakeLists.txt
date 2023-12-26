cmake_minimum_required(VERSION 3.21)

project(mat-json VERSION 2.0.0)

# This option is only useful for Geode itself
if (DEFINED MAT_JSON_AS_INTERFACE AND MAT_JSON_AS_INTERFACE)
    add_library(mat-json INTERFACE)
    target_include_directories(mat-json INTERFACE include)

    add_library(mat-json-impl INTERFACE)

    target_link_libraries(mat-json-impl mat-json)
    target_sources(mat-json-impl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/json.cpp)
else()
    add_library(mat-json src/json.cpp)

    target_compile_features(mat-json PUBLIC cxx_std_20)
    
    target_include_directories(mat-json PUBLIC include)
endif()

if (PROJECT_IS_TOP_LEVEL)
    add_subdirectory(test)
endif()
