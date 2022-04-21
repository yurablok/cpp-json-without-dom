# Call it before the main `project(...)`
macro(check_build_directory)
    if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
        message(FATAL_ERROR "In-source builds not allowed. Please use a build directory.")
    else()
        message("${CMAKE_BINARY_DIR}")
    endif()
    if("${CMAKE_BUILD_TYPE}" STREQUAL "")
        set(default_build_type "RelWithDebInfo")
        set(CMAKE_BUILD_TYPE "RelWithDebInfo")
    endif()
endmacro(check_build_directory)

# Call it after the main `project(...)` and before the main `add_subdirectory(...)`
macro(init_project)
    if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "(arm|ARM).*")
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(BUILD_ARCH "arm64")
        else()
            set(BUILD_ARCH "arm32")
        endif()
    else()
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(BUILD_ARCH "x64")
        else()
            set(BUILD_ARCH "x32")
        endif()
    endif()

    if("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
        set(BUILD_TYPE_ "RelNoDebInfo")
    elseif("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
        set(BUILD_TYPE_ "Release")
    else()
        set(BUILD_TYPE_ "${CMAKE_BUILD_TYPE}")
    endif()

    if(UNIX)
        if(APPLE)
            set(BUILD_PLATFORM "Apple")
        else()
            set(BUILD_PLATFORM "Linux")
        endif()
    elseif(WIN32)
        set(BUILD_PLATFORM "Windows")
    else()
        set(BUILD_PLATFORM "Unknown")
    endif()

    set(BUILD_FOLDER "${BUILD_ARCH}-${BUILD_TYPE_}-${BUILD_PLATFORM}")

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${BUILD_FOLDER}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${BUILD_FOLDER}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${BUILD_FOLDER}")

    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/workdir")

    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "(GNU|Clang)")
        # -O3 -g0   3.4 MB  default Release
        # -O3 -g1   9.5 MB
        # -O2 -g1   9.3 MB
        # -O2 -g2  41.0 MB  default RelWithDebInfo
        # -O0 -g2  35.0 MB  default Debug
        # Level 1 produces minimal information, enough for making backtraces in parts
        # of the program that you don’t plan to debug. This includes descriptions of
        # functions and external variables, and line number tables, but no information
        # about local variables.
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g1 -DNDEBUG")
        
        if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
            add_compile_options(-fdiagnostics-color=always)
        elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            add_compile_options(-fcolor-diagnostics)
        endif()

    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        #set(CMAKE_CXX_FLAGS_RELEASE = "/MD /O2 /Ob2 /DNDEBUG")

        # The /Zi option produces a separate PDB file that contains all the symbolic
        # debugging information for use with the debugger. The debugging information
        # isn't included in the object files or executable, which makes them much smaller.
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MD /Zi /O2 /Ob2 /DNDEBUG")
        
    else()
        message(FATAL_ERROR "Unknown compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endmacro(init_project)
