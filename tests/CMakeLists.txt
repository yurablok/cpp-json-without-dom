cmake_minimum_required(VERSION 3.12)
include("CMakeUtils.cmake")

check_build_directory()

project(tests VERSION 0.0.1 LANGUAGES CXX C)

init_project()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
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

target_sources(${PROJECT_NAME} PRIVATE
    "tests.cpp"
)
