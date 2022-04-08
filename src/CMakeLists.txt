set(CMAKE_CXX_STANDARD 14)

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

set (TDSHELL_SOURCE
    main.cpp
    tdcore.h
    tdcore.cpp
)

add_executable (tdshell ${TDSHELL_SOURCE} ${TL_TD_JSON_AUTO} )

set_source_files_properties(${TL_TD_JSON_AUTO} PROPERTIES GENERATED TRUE)
add_dependencies(tdshell tl_generate_json)
target_link_libraries (tdshell tdclient ${ZLIB_LIBRARIES} -lconfig++ ${LUA_LIBRARIES} -lpthread -lcrypto -lssl )
install (TARGETS tdshell RUNTIME DESTINATION bin)