cmake_minimum_required(VERSION 3.12)
include("CMakeUtils.cmake")

check_build_directory()

project(tests VERSION 0.0.1 LANGUAGES CXX C)

init_project("tests" "")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_executable(${PROJECT_NAME})

fetch_git("deps/cpp-adaptive-benchmark"
    https://github.com/yurablok/cpp-adaptive-benchmark.git 12d8f7d)

fetch_git("deps/minijson_reader"
    https://github.com/giacomodrago/minijson_reader.git 1.0)

fetch_git("deps/minijson_writer"
    https://github.com/giacomodrago/minijson_writer.git 08cc049)

fetch_git("deps/nlohmann_json"
    https://github.com/nlohmann/json.git v3.11.2)

fetch_git("deps/rapidjson"
    https://github.com/Tencent/rapidjson.git 012be85) # 232389d

fetch_git("deps/string-view"
    https://github.com/martinmoene/string-view-lite 5b1d95f) # f7aca36

fetch_git("deps/switch-str"
    https://github.com/yurablok/switch-str.git e2a77dd)

fetch_git("deps/variant"
    https://github.com/mpark/variant 23cb94f)

target_include_directories(${PROJECT_NAME} PRIVATE
    "deps"
    "deps/string-view/include/nonstd"
    "deps/variant/include/mpark"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    _CRT_SECURE_NO_WARNINGS
)

target_sources(${PROJECT_NAME} PRIVATE
    "tests.cpp"
)
