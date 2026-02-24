cmake_minimum_required(VERSION 3.14)
project(serdes)

# Set C++ standard globally - SystemC is built with C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add cmake modules path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# ============================================================================
# Find SystemC and SystemC-AMS
# ============================================================================
# Usage:
#   1. Set environment variables:
#      export SYSTEMC_HOME=/path/to/systemc-2.3.4
#      export SYSTEMC_AMS_HOME=/path/to/systemc-ams-2.3.4
#
#   2. Or use CMake options:
#      cmake -DSYSTEMC_HOME=/path/to/systemc -DSYSTEMC_AMS_HOME=/path/to/systemc-ams ..
#
#   3. Or install to standard location:
#      - /usr/local/systemc-2.3.4
#      - /opt/systemc-2.3.4
#      - macOS Homebrew: /opt/homebrew/opt/systemc
# ============================================================================

#find_package(SystemC REQUIRED)
list(APPEND CMAKE_PREFIX_PATH ${SYSTEMC_HOME})
find_package(SystemCLanguage REQUIRED)


#if(NOT SystemCLanguage_FOUND OR NOT SystemCAMS_FOUND)
if(NOT SystemCLanguage_FOUND)
    message(FATAL_ERROR "
SystemC or SystemC-AMS not found!

Please install SystemC/SystemC-AMS and set one of:

1. Environment variables (recommended):
   export SYSTEMC_HOME=/path/to/systemc-2.3.4
   export SYSTEMC_AMS_HOME=/path/to/systemc-ams-2.3.4

2. CMake options:
   cmake -DSYSTEMC_HOME=/path/to/systemc -DSYSTEMC_AMS_HOME=/path/to/systemc-ams ..

3. Install to standard location:
   - /usr/local/systemc-2.3.4
   - /opt/systemc-2.3.4
   - macOS Homebrew: /opt/homebrew/opt/systemc

See README.md for installation instructions.
")
endif()

# ============================================================================
# Include directories
# ============================================================================
include_directories(
	#${SYSTEMC_HOME}/include
    #${SYSTEMC_AMS_HOME}/include
    /home/software/systemc-2.3.4/include
    /home/software/systemc-ams-2.3.4/include
    include
    include/ams
    include/common
    include/de
    tb/top
    tb/periphery
)

# ============================================================================
# Source files
# ============================================================================
file(GLOB_RECURSE AMS_SOURCES src/ams/*.cpp)
file(GLOB_RECURSE DE_SOURCES src/de/*.cpp)

# ============================================================================
# Create library
# ============================================================================
add_library(serdes_lib STATIC ${AMS_SOURCES} ${DE_SOURCES})

# Link SystemC libraries
target_link_libraries(serdes_lib
    SystemC::systemc
	#SystemC::systemc-ams
	#${SYSTEMC_AMS_HOME}/lib-linux64/libsystemc-ams.a
    /home/software/systemc-ams-2.3.4/lib-linux64/libsystemc-ams.a
)

# ============================================================================
# Add subdirectories
# ============================================================================
add_subdirectory(tb)

# Optionally build tests if BUILD_TESTING is enabled
option(BUILD_TESTING "Build unit tests" OFF)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Installation
# ============================================================================
install(TARGETS serdes_lib
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# ============================================================================
# Print summary
# ============================================================================
message(STATUS "")
message(STATUS "========================================================================")
message(STATUS "SerDes Project Configuration Summary")
message(STATUS "========================================================================")
message(STATUS "C++ Standard:      ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "Install Prefix:    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "SystemC_HOME:      ${SYSTEMC_HOME}")
message(STATUS "SystemC-AMS_HOME:  ${SYSTEMC_AMS_HOME}")
message(STATUS "Build Tests:       ${BUILD_TESTING}")
message(STATUS "========================================================================")
message(STATUS "")
