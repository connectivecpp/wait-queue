/** @file
 *  
 *  @brief Example code demonstrating use of @c chops::wait_queue.
 *  See @c threaded_wait_shared_demo.cpp for multithreaded example. 
 * 
 *  @author Thurman Gillespy
 * 
 *  Copyright (c)2019 by Thurman Gillespy
 *  3/22/19
 *
 *  Minor changes May 2024 by Cliff Green to match new @c wait_queue API.
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 * 
 *  Sample make file: 
 *  g++ -std=c++17 -I ~/Projects/utility-rack/include/ wait_queue_demo.cpp
 * 
 */

#include <iostream>
#include <ios> // std::boolalpha
#include <cstdlib> // EXIT_SUCCESS
#include <string>
#include <optional> // std::optional

#include "queue/wait_queue.hpp"

// print queue stats
void queueState(const chops::wait_queue<int>& wq) {
    
    std::cout << std::boolalpha;
    std::cout << "wait queue contains " << wq.size() << " elements" << std::endl;
    std::cout << "wait queue is empty: " << wq.empty() << std::endl;
    std::cout << "wait queue stop requested: " << wq.stop_requested() << std::endl;
    std::cout << std::endl; 
}

// tasty lambda utilities
constexpr auto printStr = [] (const std::string& s) { std::cout << s << std::endl; };
constexpr auto printLn = [] () { std::cout << std::endl; };

int main() {

    // create a new (empty) queue
    printStr("create new wait queue");
    printLn();
    chops::wait_queue<int> wq;
    queueState(wq);
    
    // put some values in the queue
    printStr("pushing elements onto the queue...");
    wq.push(42); wq.push(22); wq.push(102); wq.push(-12); wq.push(17);
    queueState(wq);

    // print all the values
    printStr("print all the values in queue");
    wq.apply([] (int elem) { std::cout << elem << " "; });
    printLn();

    // remove the elements
    printStr("pop (and remove) each element from the queue");
    auto num_elements = wq.size();
    while (num_elements-- > 0) {
        auto result = wq.try_pop(); // std::optional<int>
        if (result) {
            std::cout << result.value() << " ";
        }
    }
    printLn();
    
    queueState(wq);

    return EXIT_SUCCESS;
}
