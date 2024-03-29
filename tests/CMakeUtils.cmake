# CMake Utils
# https://github.com/yurablok/cmake-cpp-template
#
# History:
# v0.4  2023-Feb-03     Added `fetch_git`.
# v0.3  2023-Jan-24     Added `add_option`.
# v0.2  2022-Dec-24     Added support for Windows ARM64.
# v0.1  2022-Oct-18     First release.

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

# Call it after the main `project(...)` and before any `add_subdirectory(...)`
# Example:
#   init_project("client --ip=localhost" "server")
function(init_project)
    if(NOT "${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
        message(WARNING "CMakeUtils.cmake is not in the root.")
        return()
    endif()

    cmake_parse_arguments(arg "" "" "" ${ARGN})
    if(NOT DEFINED arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "At least one target must be specified.")
    endif()

    if(NOT "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" STREQUAL "")
        set(CMAKE_TARGET_ARCH ${CMAKE_CXX_COMPILER_ARCHITECTURE_ID})
    else()
        set(CMAKE_TARGET_ARCH ${CMAKE_SYSTEM_PROCESSOR})
    endif()
    message("CMAKE_TARGET_ARCH: ${CMAKE_TARGET_ARCH}")
    if("${CMAKE_TARGET_ARCH}" MATCHES "(arm|ARM|aarch).*")
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
        set(BUILD_TYPE "RelNoDebInfo")
    elseif("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
        set(BUILD_TYPE "Release")
    else()
        set(BUILD_TYPE "${CMAKE_BUILD_TYPE}")
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

    set(BUILD_FOLDER "${BUILD_ARCH}-${BUILD_TYPE}-${BUILD_PLATFORM}")
    message("BUILD_FOLDER: ${BUILD_FOLDER}")

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${BUILD_FOLDER}" PARENT_SCOPE)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${BUILD_FOLDER}" PARENT_SCOPE)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/${BUILD_FOLDER}" PARENT_SCOPE)

    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/build/.cmake")
    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/workdir")

    if("${CMAKE_SYSROOT}" STREQUAL "")
        #NOTE: clangd linter config
        file(WRITE ".clangd" "CompileFlags:\n  CompilationDatabase: build/.cmake/${BUILD_FOLDER}\n")
        set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    endif()

    if (NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        #NOTE: Link-Time Global Optimization
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE PARENT_SCOPE)
    endif()

    if(MSVC)
        if(NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            add_compile_options(
                /utf-8 # Set source and execution character sets to UTF-8
                /sdl # Enable Additional Security Checks
                /MP # Build with Multiple Processes
                /permissive- # Standards conformance
            )
        else()
            add_compile_options(-fcolor-diagnostics)
        endif()

        if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
            add_compile_options(
                /JMC # Just My Code Debugging
                /ZI # Debug Information with Edit and Continue
            )
            add_link_options(
                /INCREMENTAL # For Edit and Continue
            )
        endif()

        #set(CMAKE_CXX_FLAGS_RELEASE = "/MD /O2 /Ob2 /DNDEBUG")

        # The /Zi option produces a separate PDB file that contains all the symbolic
        # debugging information for use with the debugger. The debugging information
        # isn't included in the object files or executable, which makes them much smaller.
        set(CMAKE_C_FLAGS_RELWITHDEBINFO "/MD /Zi /O2 /Ob2 /DNDEBUG" PARENT_SCOPE)
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MD /Zi /O2 /Ob2 /DNDEBUG" PARENT_SCOPE)

        #NOTE: When changing the Qt5_DIR, you may need to manually delete CMakeCache.txt
        __find_msvc_qt5("C;D;E" "5.15.2")
        __write_msvs_launch_vs_json("${arg_UNPARSED_ARGUMENTS}")
        __crutch_for_msvs_bug_with_merges()

    elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "(GNU|Clang)")
        # -O3 -g0   3.4 MB  default Release
        # -O3 -g1   9.5 MB
        # -O2 -g1   9.3 MB
        # -O2 -g2  41.0 MB  default RelWithDebInfo
        # -O0 -g2  35.0 MB  default Debug
        # Level 1 produces minimal information, enough for making backtraces in parts
        # of the program that you don’t plan to debug. This includes descriptions of
        # functions and external variables, and line number tables, but no information
        # about local variables.
        set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g1 -DNDEBUG" PARENT_SCOPE)
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g1 -DNDEBUG" PARENT_SCOPE)
        
        if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
            add_compile_options(-fdiagnostics-color=always)
        elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            add_compile_options(-fcolor-diagnostics)
        endif()

        __find_gcc_qt5("5.15.2")

    else()
        message(FATAL_ERROR "Unknown compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()

    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED OFF)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_C_STANDARD 17)
    set(CMAKE_C_STANDARD_REQUIRED OFF)
    set(CMAKE_C_EXTENSIONS OFF)

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(Qt5Path "${Qt5x64Path}" PARENT_SCOPE)
    else()
        set(Qt5Path "${Qt5x32Path}" PARENT_SCOPE)
    endif()
    if(Qt5_DIR)
        set(Qt5_DIR "${Qt5_DIR}" PARENT_SCOPE)
    endif()
    set(BUILD_ARCH "${BUILD_ARCH}" PARENT_SCOPE)
    set(BUILD_TYPE "${BUILD_TYPE}" PARENT_SCOPE)
    set(BUILD_PLATFORM "${BUILD_PLATFORM}" PARENT_SCOPE)
    set(BUILD_FOLDER "${BUILD_FOLDER}" PARENT_SCOPE)
endfunction(init_project)


# @param [0]                Type: BOOL | ENUM | STRING | DIR | FILE
# @param [1]                Option's name. Prefix "OPTION_" will be added.
# @param [2] (optional)     Default variant
# @param [n] (optional)     Other variants
# @param TARGETS            Targets
# @param COMMENT (optional) Comment
function(add_option)
    set(argIdx -1)
    set(optionDefault "")
    set(optionVariants "")
    set(isTargets FALSE)
    set(optionTargets "")
    set(isComment FALSE)
    set(optionComment "")
    foreach(arg ${ARGN})
        math(EXPR argIdx "${argIdx}+1")
        if(${argIdx} EQUAL 0)
            set(optionType "${arg}")

        elseif(${argIdx} EQUAL 1)
            set(optionName "OPTION_${arg}")

        elseif("${arg}" STREQUAL "TARGETS")
            set(isTargets TRUE)
            set(isComment FALSE)

        elseif("${arg}" STREQUAL "COMMENT")
            set(isTargets FALSE)
            set(isComment TRUE)

        elseif(${isTargets})
            list(APPEND optionTargets "${arg}")

        elseif(${isComment})
            list(APPEND optionComment "${arg}")

        else()
            if(${argIdx} EQUAL 2)
                set(optionDefault "${arg}")
            endif()
            list(APPEND optionVariants "${arg}")
        endif()
    endforeach()
    string(REPLACE ";" "\n " optionComment "${optionComment}")

    if("${optionName}" STREQUAL "")
        message(FATAL_ERROR "add_option: <NAME> must be specified.")

    elseif("${optionType}" STREQUAL "")
        message(FATAL_ERROR "add_option: <TYPE> must be specified.")

    elseif("${optionTargets}" STREQUAL "")
        message(FATAL_ERROR "add_option: <TARGETS> must be specified.")

    elseif("${optionType}" STREQUAL "BOOL")
        if(NOT "${optionDefault}" MATCHES "^(ON|OFF|TRUE|FALSE|YES|NO)$")
            message(FATAL_ERROR "add_option: BOOL: <DEFAULT> must be one of (ON | OFF | TRUE | FALSE | YES | NO).")
        endif()
        list(LENGTH optionVariants size)
        if(NOT ${size} EQUAL 1)
            message(FATAL_ERROR "add_option: BOOL: too many defaults.")
        endif()
        if("${optionComment}" STREQUAL "")
            set(optionComment " Boolean option")
        endif()

        if(NOT "${${optionName}}" STREQUAL "")
            set(optionDefault ${${optionName}})
        endif()
        set(${optionName} ${optionDefault} CACHE BOOL "${optionComment}" FORCE)

        foreach(optionTarget ${optionTargets})
            if(${optionDefault})
                target_compile_definitions(${optionTarget} PRIVATE ${optionName}=1)
            else()
                target_compile_definitions(${optionTarget} PRIVATE ${optionName}=0)
            endif()
        endforeach()

    elseif("${optionType}" STREQUAL "ENUM")
        if("${optionVariants}" MATCHES ".* .*")
            message(FATAL_ERROR "add_option: ENUM: variants must not contain spaces in their names.")
        endif()
        list(LENGTH optionVariants size)
        if(${size} LESS 2)
            message(FATAL_ERROR "add_option: ENUM: too few variants.")
        endif()
        if(NOT "${optionComment}" STREQUAL "")
            string(APPEND optionComment "\n")
        endif()
        string(APPEND optionComment " Enum variants: ${optionVariants}")

        if(NOT "${${optionName}}" STREQUAL "")
            set(optionDefault ${${optionName}})
            list(FIND optionVariants ${optionDefault} result)
            if(${result} LESS 0)
                message(FATAL_ERROR "add_option: ENUM: unknown variant ${optionDefault}.")
            endif()
        endif()
        set(${optionName} ${optionDefault} CACHE STRING "${optionComment}" FORCE)
        set_property(CACHE ${optionName} PROPERTY STRINGS ${optionVariants})

        foreach(optionTarget ${optionTargets})
            target_compile_definitions(${optionTarget} PRIVATE ${optionName}="${optionDefault}")
            target_compile_definitions(${optionTarget} PRIVATE ${optionName}_${optionDefault}=1)
        endforeach()

    elseif("${optionType}" STREQUAL "STRING")
        list(LENGTH optionVariants size)
        if(${size} GREATER 1)
            message(FATAL_ERROR "add_option: STRING: too many defaults.")
        endif()
        if("${optionComment}" STREQUAL "")
            set(optionComment " String option")
        endif()

        if(DEFINED ${optionName})
            set(optionDefault ${${optionName}})
        endif()
        set(${optionName} ${optionDefault} CACHE STRING "${optionComment}" FORCE)

        foreach(optionTarget ${optionTargets})
            target_compile_definitions(${optionTarget} PRIVATE ${optionName}="${optionDefault}")
        endforeach()

    elseif("${optionType}" STREQUAL "DIR")
        list(LENGTH optionVariants size)
        if(${size} GREATER 1)
            message(FATAL_ERROR "add_option: DIR: too many defaults.")
        endif()
        if("${optionComment}" STREQUAL "")
            set(optionComment " Directory option")
        endif()

        if(DEFINED ${optionName})
            set(optionDefault ${${optionName}})
        endif()
        set(${optionName} ${optionDefault} CACHE PATH "${optionComment}" FORCE)

        foreach(optionTarget ${optionTargets})
            target_compile_definitions(${optionTarget} PRIVATE ${optionName}="${optionDefault}")
        endforeach()

    elseif("${optionType}" STREQUAL "FILE")
        list(LENGTH optionVariants size)
        if(${size} GREATER 1)
            message(FATAL_ERROR "add_option: FILE: too many defaults.")
        endif()
        if("${optionComment}" STREQUAL "")
            set(optionComment " File option")
        endif()

        if(DEFINED ${optionName})
            set(optionDefault ${${optionName}})
        endif()
        set(${optionName} ${optionDefault} CACHE FILEPATH "${optionComment}" FORCE)

        foreach(optionTarget ${optionTargets})
            target_compile_definitions(${optionTarget} PRIVATE ${optionName}="${optionDefault}")
        endforeach()

    else()
        message(FATAL_ERROR "add_option: <TYPE> must be one of (BOOL | ENUM | STRING | DIR | FILE).")
    endif()

endfunction(add_option)


# @param directory  Target directory to download sources.
# @param address    Git-compatible address of a repository.
# @param tag        Desired branch | tag | hash.
function(fetch_git directory address tag)
    get_filename_component(absolutePath ${directory} ABSOLUTE)
    file(RELATIVE_PATH relativePath ${CMAKE_SOURCE_DIR} ${absolutePath})
    message("fetch_git: checking \"${relativePath}\"...")

    if(NOT EXISTS "${absolutePath}/.git/HEAD")
        message("fetch_git: downloading \"${relativePath}\"...")

        get_filename_component(absoluteParentPath "${absolutePath}/../" ABSOLUTE)
        file(MAKE_DIRECTORY ${absoluteParentPath})

        string(LENGTH ${absoluteParentPath} length)
        math(EXPR length "${length}+1")
        string(SUBSTRING ${absolutePath} ${length} -1 folder)

        execute_process(
            WORKING_DIRECTORY ${absoluteParentPath}
            COMMAND git clone --branch ${tag} --single-branch --recurse-submodules ${address} ${folder}
            OUTPUT_VARIABLE output
            ERROR_VARIABLE error
            RESULT_VARIABLE result
        )
        # Normal + branch | tag
        #   output=
        #   error=Cloning into 'repo123'...
        #   result=0
        # Normal + hash
        #   output=
        #   error=Cloning into 'repo123'...
        #         warning: Could not find remote branch 1234567 to clone.
        #         fatal: Remote branch 1234567 not found in upstream origin
        #   result=128
        # Error 128 (not empty)
        #   output=
        #   error=fatal: destination path 'repo123' already exists and is not an empty directory.
        #   result=128
        # Error 128 (unreachable)
        #   output=
        #   error=Cloning into 'repo123'...
        #         fatal: unable to access 'https://....': Could not resolve host: ....
        #   result=128
        if(NOT ${result} EQUAL 0)
            string(FIND "${error}" "not found in upstream" result)
            if(${result} GREATER 0)
                execute_process(
                    WORKING_DIRECTORY ${absoluteParentPath}
                    COMMAND git clone --single-branch --recurse-submodules ${address} ${folder}
                    OUTPUT_VARIABLE output
                    ERROR_VARIABLE error
                    RESULT_VARIABLE result
                )
                if(NOT ${result} EQUAL 0)
                    message(FATAL_ERROR "fetch_git: ${error}")
                endif()

                execute_process(
                    WORKING_DIRECTORY ${absolutePath}
                    COMMAND git checkout --recurse-submodules ${tag}
                    OUTPUT_VARIABLE output
                    ERROR_VARIABLE error
                    RESULT_VARIABLE result
                )
                # Normal + hash
                #   output=
                #   error=Note: switching to '1234567'.
                #         You are in 'detached HEAD' state.
                #   result=0
                # Error 1
                #   output=
                #   error=error: pathspec '1234567' did not match any file(s) known to git
                #   result=1
            endif()
        endif()

        if(NOT ${result} EQUAL 0)
            message(FATAL_ERROR "fetch_git: ${error}")
        endif()

    else()
        execute_process(
            WORKING_DIRECTORY ${absolutePath}
            COMMAND git checkout --recurse-submodules ${tag}
            OUTPUT_VARIABLE output
            ERROR_VARIABLE error
            RESULT_VARIABLE result
        )
        # Error 1 (no match)
        #   output=
        #   error=error: pathspec '2.5.1' did not match any file(s) known to git
        #   result=1
        if(NOT ${result} EQUAL 0)
            message("fetch_git: fetching \"${relativePath}\"...")
            file(REMOVE "${absolutePath}/.git/index.lock")

            execute_process(
                WORKING_DIRECTORY ${absolutePath}
                COMMAND git fetch --all --tags --recurse-submodules
                OUTPUT_VARIABLE output
                ERROR_VARIABLE error
                RESULT_VARIABLE result
            )
            if(NOT ${result} EQUAL 0)
                message(FATAL_ERROR "fetch_git: ${error}")
            endif()

            execute_process(
                WORKING_DIRECTORY ${absolutePath}
                COMMAND git checkout --recurse-submodules --force ${tag}
                OUTPUT_VARIABLE output
                ERROR_VARIABLE error
                RESULT_VARIABLE result
            )
        endif()

        if(NOT ${result} EQUAL 0)
            message(FATAL_ERROR "fetch_git: ${error}")
        endif()

    endif()

    message("fetch_git: \"${relativePath}\" is ready")
endfunction(fetch_git)


# @param SOURCES    Directory where lupdate will look for C++ sources
# @param TS_FILES   List of generated *.ts files
# @param QM_DIR     Directory for generated *.qm files
function(qt5_create_ts_and_qm)
    set(options)
    set(oneValueArgs SOURCES QM_DIR)
    set(multiValueArgs TS_FILES)
    cmake_parse_arguments(arg "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    file(MAKE_DIRECTORY "${arg_QM_DIR}")

    foreach(tsFile ${arg_TS_FILES})
        get_filename_component(tsFileName "${tsFile}" NAME_WLE)
        #add_custom_command(
        #    TARGET ${PROJECT_NAME} PRE_BUILD
        #    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        #    COMMAND "${Qt5Path}/bin/lupdate"
        #    ARGS -locations none ${arg_SOURCES} -ts ${tsFile}
        #)
        #add_custom_command(
        #    TARGET ${PROJECT_NAME} PRE_BUILD
        #    DEPENDS "${tsFile}"
        #    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        #    COMMAND "${Qt5Path}/bin/lrelease"
        #    ARGS ${tsFile} -qm ${arg_QM_DIR}/${tsFileName}.qm
        #)

        add_custom_command(
            TARGET ${PROJECT_NAME} PRE_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DRUN=qt5_create_ts_and_qm

                -DLUPDATE=${Qt5Path}/bin/lupdate
                -DLRELEASE=${Qt5Path}/bin/lrelease
                -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DDIRECTORY=${arg_SOURCES}
                -DTSFILE=${tsFile}
                -DQMFILE=${arg_QM_DIR}/${tsFileName}.qm

                -P ${CMAKE_SOURCE_DIR}/CMakeUtils.cmake
        )
    endforeach()
endfunction(qt5_create_ts_and_qm)

function(__qt5_create_ts_and_qm_impl)
    # lupdate directory -ts file.ts -locations none
    execute_process(
        WORKING_DIRECTORY ${WORKDIR}
        COMMAND ${LUPDATE} -locations none ${DIRECTORY} -ts ${TSFILE}
        OUTPUT_VARIABLE result
    )
    if("${result}" STREQUAL "")
        return()
    endif()

    # Found 0 source text(s) (0 new and 0 already existing)
    string(REGEX MATCH "([0-9]+)[ a-z()]+([0-9]+)[ a-z()]+([0-9]+)" result ${result})
    message("lupdate: ${CMAKE_MATCH_1} found, ${CMAKE_MATCH_2} new, ${CMAKE_MATCH_3} exists")

    if(EXISTS "${WORKDIR}/${QMFILE}" AND "${CMAKE_MATCH_2}" STREQUAL "0")
        #NOTE: Remove *.qm files after editing *.ts files!
        return()
    endif()

    # lrelease file.ts -qm file.qm
    execute_process(
        WORKING_DIRECTORY ${WORKDIR}
        COMMAND ${LRELEASE} ${TSFILE} -qm ${QMFILE}
        OUTPUT_VARIABLE result
    )
    if("${result}" STREQUAL "")
        return()
    endif()

    # Generated 0 translation(s) (0 finished and 0 unfinished)
    #string(REGEX MATCH "([0-9]+)[ a-z()]+([0-9]+)[ a-z()]+([0-9]+)" result ${result})
    message("lrelease: ${QMFILE}")
endfunction(__qt5_create_ts_and_qm_impl)


function(__find_gcc_qt5 qtVersion)
    set(qtPath "~/Qt/${qtVersion}/gcc_64")
    if(EXISTS "${qtPath}")
        set(Qt5x64Path "${qtPath}" PARENT_SCOPE)
    else()
        set(qtPath "")
    endif()

    if(NOT "${qtPath}" STREQUAL "")
        if(${BUILD_ARCH} STREQUAL "x64")
            set(Qt5_DIR "${qtPath}/lib/cmake/Qt5" PARENT_SCOPE)
        endif()
        message("Qt5x64Path: ${qtPath}")
    endif()
endfunction(__find_gcc_qt5)

function(__find_msvc_qt5 drives qtVersion)
    foreach(drive ${drives})
        set(qtPath "${drive}:/Qt/${qtVersion}/msvc2019")
        if(EXISTS "${qtPath}")
            set(Qt5x32Path "${qtPath}" PARENT_SCOPE)
            break()
        endif()
        set(qtPath "${drive}:/Qt/${qtVersion}/msvc2017")
        if(EXISTS "${qtPath}")
            set(Qt5x32Path "${qtPath}" PARENT_SCOPE)
            break()
        endif()
        set(qtPath "")
    endforeach(drive)
    if(NOT "${qtPath}" STREQUAL "")
        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(Qt5_DIR "${qtPath}/lib/cmake/Qt5" PARENT_SCOPE)
        endif()
        message("Qt5x32Path: ${qtPath}")
    endif()

    foreach(drive ${drives})
        set(qtPath "${drive}:/Qt/${qtVersion}/msvc2019_64")
        if(EXISTS "${qtPath}")
            set(Qt5x64Path "${qtPath}" PARENT_SCOPE)
            break()
        endif()
        set(qtPath "${drive}:/Qt/${qtVersion}/msvc2017_64")
        if(EXISTS "${qtPath}")
            set(Qt5x64Path "${qtPath}" PARENT_SCOPE)
            break()
        endif()
        set(qtPath "")
    endforeach(drive)
    if(NOT "${qtPath}" STREQUAL "")
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(Qt5_DIR "${qtPath}/lib/cmake/Qt5" PARENT_SCOPE)
        endif()
        message("Qt5x64Path: ${qtPath}")
    endif()
endfunction(__find_msvc_qt5)


function(__write_msvs_launch_vs_json targets)
    set(cfgPath "${CMAKE_SOURCE_DIR}/.vs/launch.vs.json")

    file(WRITE "${cfgPath}" "reload") # Reload the config by using some JSON-error

    set(json "")
    set(json "${json}{\n")
    set(json "${json}  \"NOTE\": \"This file was generated by CMakeUtils.cmake\",\n")
    set(json "${json}  \"version\": \"0.2.1\",\n")
    set(json "${json}  \"configurations\": [\n")

    macro(add appPath qtPath targetName targetArgs)
        string(REPLACE "/" "\\\\" targetPath "${CMAKE_SOURCE_DIR}/build/${appPath}/${targetName}")
        set(json "${json}    {\n")
        set(json "${json}      \"type\": \"default\",\n")
        set(json "${json}      \"project\": \"CMakeLists.txt\",\n")
        set(json "${json}      \"projectTarget\": \"${targetName}.exe (${targetPath}.exe)\",\n")
        set(json "${json}      \"name\": \"${targetName}\",\n")
        set(json "${json}      \"args\": [\n")
        set(json "${json}        \"${targetArgs}\"\n")
        set(json "${json}      ],\n")
        set(json "${json}      \"currentDir\": \"\${workspaceRoot}/workdir\",\n")
        set(json "${json}      \"env\": {\n")
        set(json "${json}        \"PATH\": \"\${env.PATH};${qtPath}/bin\"\n")
        set(json "${json}      }\n")
        set(json "${json}    },\n")
    endmacro(add)

    foreach(target ${targets})
        string(REGEX MATCH "^([_a-zA-Z0-9]+)[ ]*(.*)$" matched "${target}")
        if(${CMAKE_MATCH_COUNT} EQUAL 0)
            message(FATAL_ERROR "Wrong target format (actual='${target}', expected='name args').")
        endif()

        set(targetName ${CMAKE_MATCH_1})
        set(targetArgs ${CMAKE_MATCH_2})
        #message("targetName=${targetName} targetArgs=[${targetArgs}]")

        add("x32-Debug-Windows/Debug"              "${Qt5x32Path}" "${targetName}" "${targetArgs}")
        add("x32-Release-Windows/RelWithDebInfo"   "${Qt5x32Path}" "${targetName}" "${targetArgs}")
        add("x32-RelNoDebInfo-Windows/Release"     "${Qt5x32Path}" "${targetName}" "${targetArgs}")
        add("x64-Debug-Windows/Debug"              "${Qt5x64Path}" "${targetName}" "${targetArgs}")
        add("x64-Release-Windows/RelWithDebInfo"   "${Qt5x64Path}" "${targetName}" "${targetArgs}")
        add("x64-RelNoDebInfo-Windows/Release"     "${Qt5x64Path}" "${targetName}" "${targetArgs}")
        add("arm64-Debug-Windows/Debug"            "${Qt5x64Path}" "${targetName}" "${targetArgs}")
        add("arm64-Release-Windows/RelWithDebInfo" "${Qt5x64Path}" "${targetName}" "${targetArgs}")
        add("arm64-RelNoDebInfo-Windows/Release"   "${Qt5x64Path}" "${targetName}" "${targetArgs}")
    endforeach()

    set(json "${json}  ]\n")
    set(json "${json}}\n")

    file(WRITE "${cfgPath}" "${json}")
endfunction(__write_msvs_launch_vs_json)


# CPU leak by:
#  "C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/ServiceHub/Hosts/ServiceHub.Host.Dotnet.arm64/ServiceHub.IndexingService.exe"
# Memory leak in:
#  "C:/..../myproject/.vs/myproject/FileContentIndex/merges"
# https://stackoverflow.com/questions/72237599/how-to-disable-that-new-filecontentindex-folder-and-vsidx-files-in-vs-2022
# "C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/Editor/ServiceHub/Indexing.servicehub.service.json"
function(__crutch_for_msvs_bug_with_merges)
    get_filename_component(absolutePatentPath "${CMAKE_SOURCE_DIR}/../" ABSOLUTE)
    set(absolutePath "${CMAKE_SOURCE_DIR}")
    string(LENGTH ${absolutePatentPath} length)
    math(EXPR length "${length}+1")
    string(SUBSTRING ${absolutePath} ${length} -1 folder)
    set(mergesPath "${CMAKE_SOURCE_DIR}/.vs/${folder}/FileContentIndex/merges")

    if(EXISTS "${mergesPath}/")
        file(REMOVE_RECURSE "${mergesPath}/")
        file(TOUCH "${mergesPath}")
    endif()
endfunction(__crutch_for_msvs_bug_with_merges)


function(copy_release_file_to_workdir frompath topath)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        return()
    endif()

    if(MSVC)
        set(fullfrom "${CMAKE_SOURCE_DIR}/build/${BUILD_FOLDER}/${CMAKE_BUILD_TYPE}/${frompath}")
    else()
        set(fullfrom "${CMAKE_SOURCE_DIR}/build/${BUILD_FOLDER}/${frompath}")
    endif()
    set(fullto "${CMAKE_SOURCE_DIR}/workdir/bin_${BUILD_ARCH}/${topath}")

    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${fullfrom}" "${fullto}"
    )
endfunction(copy_release_file_to_workdir)

macro(copy_release_app_to_workdir basename)
    if(WIN32)
        copy_release_file_to_workdir("${basename}.exe" "${basename}.exe")
    else()
        copy_release_file_to_workdir("${basename}" "${basename}")
    endif()
endmacro(copy_release_app_to_workdir)

macro(copy_release_lib_to_workdir basename)
    if(WIN32)
        copy_release_file_to_workdir("${basename}.dll" "${basename}.dll")
    else()
        copy_release_file_to_workdir("lib${basename}.so" "${basename}.so")
    endif()
endmacro(copy_release_lib_to_workdir)


if("${RUN}" STREQUAL "qt5_create_ts_and_qm")
    __qt5_create_ts_and_qm_impl()
endif()
