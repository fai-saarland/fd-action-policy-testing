cmake_minimum_required(VERSION 3.20.0)
# For Windows we require CMake 3.12, but this is currently not
# available for Ubuntu 18.04.

if(NOT FAST_DOWNWARD_MAIN_CMAKELISTS_READ)
    message(
        FATAL_ERROR
        "Run cmake on the CMakeLists.txt in the 'src' directory, "
        "not the one in 'src/search'. Please delete CMakeCache.txt "
        "from the current directory and restart cmake.")
endif()


## == Project ==

project(downward)

# TODO: use multi-line strings to break up the long string when we switch to CMake 3.
# https://stackoverflow.com/questions/7637539/how-to-split-strings-across-multiple-lines-in-cmake
option(
  USE_GLIBCXX_DEBUG
  "Enable the libstdc++ debug mode that does additional safety checks. (On Linux systems, g++ and clang++ usually use libstdc++ for the C++ library.) The checks come at a significant performance cost and should only be enabled in debug mode. Enabling them makes the binary incompatible with libraries that are not compiled with this flag, which can lead to hard-to-debug errors."
  FALSE)

fast_downward_set_compiler_flags()
fast_downward_set_linker_flags()

# Collect source files needed for the active plugins.
include("${CMAKE_CURRENT_SOURCE_DIR}/DownwardFiles.cmake")
add_executable(downward ${PLANNER_SOURCES})



## == Required Packages ==
IF (DEFINED ENV{PATH_TORCH})
    list(APPEND CMAKE_PREFIX_PATH "$ENV{PATH_TORCH}")
ENDIF()


find_package(Boost 1.74 COMPONENTS serialization iostreams REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
target_link_libraries(downward ${Boost_LIBRARIES})


foreach(PACKAGE ${PLANNER_REQ_PACKAGES})
    find_package(${PACKAGE} REQUIRED)
    string(TOUPPER ${PACKAGE} CAPITALIZED_PACKAGE)
    foreach (P ${PACKAGE} ${CAPITALIZED_PACKAGE})
        if(${P}_INCLUDE_DIRS)
            if(${PACKAGE}_SUPPRESS_WARNINGS)
                include_directories(SYSTEM ${${P}_INCLUDE_DIRS})
            else()
                include_directories(${${P}_INCLUDE_DIRS})
            endif()
        endif()
        if(${P}_LIBRARIES)
            target_link_libraries(downward ${${P}_LIBRARIES})
        endif()
    endforeach()
endforeach()

## == Includes ==

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ext)

## == Libraries ==

# get_filename_component(CUDD_SRC_DIR policy_testing/simulations/cudd-2.5.0-64 ABSOLUTE ${CMAKE_CURRENT_SOURCE_DIR})
# target_include_directories(downward PUBLIC ${CUDD_SRC_DIR})
# set(CUDD_ROOT ${CUDD_SRC_DIR}/obj/libobj.a)
# add_custom_target(cudd ALL
#         COMMAND make objlib
#         WORKING_DIRECTORY ${CUDD_SRC_DIR}
#         COMMENT "Building CUDD library")
# add_library(cudd_library STATIC IMPORTED GLOBAL)
# set_target_properties(cudd_library PROPERTIES IMPORTED_LOCATION "${CUDD_ROOT}")
# add_dependencies(cudd_library cudd)
# target_link_libraries(downward cudd_library
#         # ${CUDD_ROOT}
#         ${CUDD_SRC_DIR}/cudd/libcudd.a
#         ${CUDD_SRC_DIR}/dddmp/libdddmp.a
#         ${CUDD_SRC_DIR}/mtr/libmtr.a
#         ${CUDD_SRC_DIR}/epd/libepd.a
#         ${CUDD_SRC_DIR}/util/libutil.a
#         ${CUDD_SRC_DIR}/st/libst.a
#         )

# On Linux, find the rt library for clock_gettime().
if(UNIX AND NOT APPLE)
    target_link_libraries(downward rt)
endif()

# On Windows, find the psapi library for determining peak memory.
if(WIN32)
    cmake_policy(SET CMP0074 NEW)
    target_link_libraries(downward psapi)
endif()

# If any enabled plugin requires an LP solver, compile with all
# available LP solvers. If no solvers are installed, the planner will
# still compile, but using heuristics that depend on an LP solver will
# cause an error. This behavior can be overwritten by setting the
# option USE_LP to false.
option(
  USE_LP
  "Compile with support for all LP solvers installed on this system."
  TRUE)

if(PLUGIN_LP_SOLVER_ENABLED AND USE_LP)
    find_package(OSI OPTIONAL_COMPONENTS Cpx Clp Grb Spx)
    if(OSI_FOUND AND (OSI_Cpx_FOUND OR OSI_Clp_FOUND OR OSI_Grb_FOUND OR OSI_Spx_FOUND))
        if(USE_GLIBCXX_DEBUG)
            message(
                FATAL_ERROR
                "To prevent incompatibilities, the option USE_GLIBCXX_DEBUG is "
                "not supported when an LP solver is used. See issue982 for details.")
        endif()
        foreach(SOLVER Cpx Clp Grb Spx)
            if(OSI_${SOLVER}_FOUND)
                string(TOUPPER ${SOLVER} TMP_SOLVER_UPPER_CASE)
                mark_as_advanced(TMP_SOLVER_UPPER_CASE)
                add_definitions("-D COIN_HAS_${TMP_SOLVER_UPPER_CASE}")
                include_directories(${OSI_${SOLVER}_INCLUDE_DIRS})
                target_link_libraries(downward ${OSI_${SOLVER}_LIBRARIES})
            endif()
        endforeach()

        # Note that basic OSI libs must be added after (!) all OSI solver libs.
        add_definitions("-D USE_LP")
        include_directories(${OSI_INCLUDE_DIRS})
        target_link_libraries(downward ${OSI_LIBRARIES})

        find_package(ZLIB REQUIRED)
        if(ZLIB_FOUND)
            include_directories(${ZLIB_INCLUDE_DIRS})
            target_link_libraries(downward ${ZLIB_LIBRARIES})
        endif()
    endif()

    if(OSI_Cpx_FOUND AND CPLEX_RUNTIME_LIBRARY)
        add_custom_command(TARGET downward POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            ${CPLEX_RUNTIME_LIBRARY}
            $<TARGET_FILE_DIR:downward>
        )
    endif()
endif()


if(PLUGIN_POLICY_TESTING_ENABLED)
    if (DEFINED ENV{PYTHON3_ROOT})
        include_directories($ENV{PYTHON3_ROOT}/include/python3.7m/)
        #target_link_libraries(downward $ENV{PYTHON3_ROOT}/lib/libpython3.7m.a -lcrypt -lpthread -ldl -lutil)
        target_link_libraries(downward $ENV{PYTHON3_ROOT}/lib/libpython3.7m.so)
    else()
        find_package(Python3 COMPONENTS Development)
        include_directories(${Python3_INCLUDE_DIRS})
        target_link_libraries(downward ${Python3_LIBRARIES})
    endif()

    if (DEFINED ENV{PHRM_ROOT})
        set(PHRM_ROOT $ENV{PHRM_ROOT})
    elseif (DEFINED ENV{PHEROMONE_ROOT})
        set(PHRM_ROOT $ENV{PHEROMONE_ROOT})
    else()
        message(SEND_ERROR "Missing pheromone library."
                "Set PRHM_ROOT or PHEROMONE_ROOT environment variable.")
    endif()

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PHRM REQUIRED grpc++ protobuf)
    include_directories(${PHRM_ROOT}/include)
    target_link_libraries(downward ${PHRM_ROOT}/libpheromone.a ${PHRM_LIBRARIES})
endif()
