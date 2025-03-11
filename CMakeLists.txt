cmake_minimum_required(VERSION 3.25)
set(CMAKE_CXX_STANDARD 20)
project(sort_n_cache VERSION 1.0 LANGUAGES C CXX)

file(GLOB SRC_DIRS RELATIVE ${CMAKE_SOURCE_DIR}/src CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*)


set(CMAKE_C_FLAGS ${COMPILATION_FALGS})
set(CMAKE_CXX_FLAGS ${COMPILATION_FALGS})

foreach(DIR ${SRC_DIRS})
    file(GLOB_RECURSE PROJ_SOURCE_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/${DIR}/*)
    file(GLOB PROJ_INCLUDE_DIR CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/${DIR}/include)

    if(PROJ_SOURCE_FILES)
        # Use directory name as target name
        set(TARGET_NAME ${DIR})

        add_executable(${TARGET_NAME} ${PROJ_SOURCE_FILES})

        if(PROJ_INCLUDE_DIR)
            target_include_directories(${TARGET_NAME} PRIVATE ${PROJ_INCLUDE_DIR})
        endif()

    endif()
endforeach()
