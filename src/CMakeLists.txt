set(CMAKE_CXX_STANDARD 17)

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    # Update submodules as needed
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()

add_subdirectory("3rdparty/td/")
include_directories ("3rdparty/td/")
include_directories ("3rdparty/td/tdactor/")
include_directories ("3rdparty/td/tdutils/")
include_directories ("3rdparty/td/tdtl/")
include_directories ("3rdparty/td/td/generate/")
include_directories($<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>/3rdparty/td/tdutils)

include_directories ("3rdparty/cli/include")
include_directories ("3rdparty/termcolor/include")
include_directories ("3rdparty/CLI11/include")

set (TDSHELL_SOURCE
    main.cpp
    tdchannel.h
    tdchannel.cpp
    tdshell.h
    tdshell.cpp
    commands.h
    commands.cpp
    utils.h
    utils.cpp
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)

add_executable (tdshell ${TDSHELL_SOURCE})
set_target_properties(tdshell PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
#target_link_libraries (tdshell tdclient ${ZLIB_LIBRARIES} -lconfig++ -lpthread -lcrypto -lssl )
target_link_libraries (tdshell tdclient tdcore tdapi -lpthread -lcrypto -lssl -lstdc++fs)

install(TARGETS tdshell RUNTIME DESTINATION bin)
