# Copyright (c) 2024 by Cliff Green
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required ( VERSION 3.14 FATAL_ERROR )

# create project
project ( wait_queue_test LANGUAGES CXX )

# add executable
add_executable ( wait_queue_test wait_queue_test.cpp )
target_compile_features ( wait_queue_test PRIVATE cxx_std_20 )

# add dependencies
include ( ../cmake/download_cpm.cmake )

CPMAddPackage ( "gh:catchorg/Catch2@3.5.4" )
CPMAddPackage ( "gh:martinmoene/ring-span-lite@0.6.0" )
# CPMAddPackage ( "gh:JustasMasiulis/circular_buffer@master" )
CPMAddPackage ( NAME circular_buffer
	        URL https://github.com/JustasMasiulis/circular_buffer/archive/refs/heads/master.zip )

set ( CMAKE_THREAD_PREFER_PTHREAD TRUE )
set ( THREADS_PREFER_PTHREAD_FLAG TRUE )
find_package ( Threads REQUIRED )

# link dependencies
target_link_libraries ( wait_queue_test PRIVATE 
	Threads::Threads wait_queue ring-span-lite circular_buffer Catch2::Catch2WithMain )

enable_testing()

add_test ( NAME run_wait_queue_test COMMAND wait_queue_test )
set_tests_properties ( run_wait_queue_test 
  PROPERTIES PASS_REGULAR_EXPRESSION "All tests passed"
  )

