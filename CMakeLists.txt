# Copyright (c) 2024-2025 by Cliff Green
#
# https://github.com/connectivecpp/wait-queue
#
# I'm still learning CMake, so improvement suggestions are always welcome.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required ( VERSION 3.14 FATAL_ERROR )

project ( wait_queue 
	  LANGUAGES CXX 
	  DESCRIPTION "A multi producer / multi consumer thread safe wait queue"
	  HOMEPAGE_URL "https://github.com/connectivecpp/wait-queue/" )

option ( WAIT_QUEUE_BUILD_TESTS "Build unit tests" OFF )
option ( WAIT_QUEUE_BUILD_EXAMPLES "Build examples" OFF )
option ( WAIT_QUEUE_INSTALL "Install header only library" OFF )

# add library targets

add_library ( wait_queue INTERFACE )
add_library ( chops::wait_queue ALIAS wait_queue )

# configure library target

target_include_directories ( wait_queue INTERFACE
                             $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
			     $<INSTALL_INTERFACE:include/> )
target_compile_features ( wait_queue INTERFACE cxx_std_20 )

# check to build unit tests
if ( ${WAIT_QUEUE_BUILD_TESTS} )
  enable_testing()
  add_subdirectory ( test )
endif ()

# check to build example code
if ( ${WAIT_QUEUE_BUILD_EXAMPLES} )
  add_subdirectory ( example )
endif ()

# check to install
if ( ${WAIT_QUEUE_INSTALL} )
  set ( CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt )
  include ( CPack )
endif ()

# end of file

