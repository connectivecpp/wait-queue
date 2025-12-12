# Copyright (c) 2024-2025 by Cliff Green
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required ( VERSION 3.14 FATAL_ERROR )

# create project
project ( wait_queue_example LANGUAGES CXX )

# add dependencies
CPMAddPackage ( "gh:connectivecpp/shared-buffer@1.0.5" )

# add executables
add_executable ( wait_queue_example wait_queue_example.cpp )
target_compile_features ( wait_queue_example PRIVATE cxx_std_20 )

add_executable ( threaded_queue_buffer_demo threaded_queue_buffer_demo.cpp )
target_compile_features ( threaded_queue_buffer_demo PRIVATE cxx_std_20 )

set ( CMAKE_THREAD_PREFER_PTHREAD TRUE )
set ( THREADS_PREFER_PTHREAD_FLAG TRUE )
find_package ( Threads REQUIRED )

# link dependencies
target_link_libraries ( wait_queue_example PRIVATE 
	Threads::Threads wait_queue )
target_link_libraries ( threaded_queue_buffer_demo PRIVATE 
	Threads::Threads shared_buffer wait_queue )

