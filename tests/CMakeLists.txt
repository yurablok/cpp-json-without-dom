cmake_minimum_required(VERSION 3.12)
include("CMakeUtils.cmake")

check_build_directory()

project(tests VERSION 0.0.1 LANGUAGES CXX C)

init_project("tests" "")

set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_executable(${PROJECT_NAME})

# minijson_reader
# header-only

# minijson_writer
# header-only

# nlohmann_json
# header-only

# rapidjson
# header-only

# switch-str
# header-only

target_include_directories(${PROJECT_NAME} PRIVATE
    "deps"
    "deps/string-view-lite/include/nonstd"
    "deps/variant/include/mpark"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    _CRT_SECURE_NO_WARNINGS
)

target_sources(${PROJECT_NAME} PRIVATE
    "tests.cpp"
)
