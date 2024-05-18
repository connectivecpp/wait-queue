/** @file
 *
 *  @brief Test scenarios for @c wait_queue class template.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2017-2024 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */


#include <utility> // std::pair
#include <functional> // std::ref
#include <vector>
#include <string>
#include <set>
#include <optional>
#include <chrono>
#include <type_traits> // std::is_arithmetic

#include <thread>
#include <future> // std::async
#include <mutex>
#include <stop_token>

#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_template_test_macros.hpp"

#include "queue/wait_queue.hpp"

// circular buffer or ring span container types to use instead of the default std::deque
#include "nonstd/ring_span.hpp"
#include "circular_buffer.hpp"

using namespace std::literals::string_literals;

constexpr int N = 40;

// WQ creation function with non-used pointer for overloading
template <typename T>
auto create_wait_queue(const std::deque<T>*) {
  return chops::wait_queue<T, std::deque<T> > { };
}

template <typename T>
auto create_wait_queue(const nonstd::ring_span<T>*) {
  static T buf[N];
  return chops::wait_queue<T, nonstd::ring_span<T> > { buf+0, buf+N };
}

template <typename T>
auto create_wait_queue(const jm::circular_buffer<T, N>*) {
  return chops::wait_queue<T, jm::circular_buffer<T, N> > { };
}


template <typename Q>
void non_threaded_push_test(Q& wq, const typename Q::value_type& val, int count) {

  REQUIRE (wq.empty());
  REQUIRE (wq.size() == 0);

  for (int i {0}; i < count; ++i) {
    REQUIRE(wq.push(val));
  }
  REQUIRE_FALSE (wq.empty());
  REQUIRE (wq.size() == count);
  for (int i {0}; i < count; ++i) {
    auto ret = wq.try_pop();
    REQUIRE(*ret == val);
  }
  REQUIRE (wq.empty());
  REQUIRE (wq.size() == 0);
}

template <typename Q>
  requires std::is_arithmetic_v<typename Q::value_type>
void non_threaded_arithmetic_test(Q& wq, int count) {

  using val_type = typename Q::value_type;

  constexpr val_type base_val { 8 };
  const val_type expected_sum = count * base_val;

  REQUIRE (wq.empty());

  for (int i {0}; i < count; ++i) {
    REQUIRE(wq.push(base_val));
  }
  val_type sum { 0 };
  wq.apply( [&sum] (const val_type& x) { sum += x; } );
  REQUIRE (sum == expected_sum);

  for (int i {0}; i < count; ++i) {
    REQUIRE(*(wq.try_pop()) == base_val);
  }
  REQUIRE (wq.empty());

  for (int i {0}; i < count; ++i) {
    wq.push(base_val+i);
  }
  for (int i {0}; i < count; ++i) {
    REQUIRE(*(wq.try_pop()) == (base_val+i));
  }
  REQUIRE (wq.size() == 0);
  REQUIRE (wq.empty());

}

template <typename T>
using set_elem = std::pair<int, T>;

template <typename T>
using test_set = std::set<set_elem<T> >;

template <typename T, typename Q>
void read_func (std::stop_token stop_tok, Q& wq, test_set<T>& s, std::mutex& mut) {
  while (true) {
    std::optional<set_elem<T> > opt_elem = wq.wait_and_pop();
    if (!opt_elem) { // empty element means request stop has been called
      return;
    }
    std::lock_guard<std::mutex> lk(mut);
    s.insert(*opt_elem);
  }
}

template <typename T, typename Q>
void write_func (std::stop_token stop_tok, Q& wq, int start, int slice, const T& val) {
  for (int i {0}; i < slice; ++i) {
    if (!wq.push(set_elem<T>{(start+i), val})) {
      // FAIL("wait queue push failed in write_func");
    }
  }
}

template <typename T, typename Q>
bool threaded_test(Q& wq, int num_readers, int num_writers, int slice, const T& val) {
  // each writer pushes slice entries
  int tot = num_writers * slice;

  test_set<T> s;
  std::mutex mut;

  std::vector<std::jthread> rd_thrs;
  for (int i {0}; i < num_readers; ++i) {
    rd_thrs.push_back( std::jthread (read_func<T, Q>, std::ref(wq), std::ref(s), std::ref(mut)) );
  }

  std::vector<std::jthread> wr_thrs;
  for (int i {0}; i < num_writers; ++i) {
    wr_thrs.push_back( std::jthread (write_func<T, Q>, std::ref(wq), (i*slice), slice, val));
  }
  // wait for writers to finish pushing vals
  for (auto& thr : wr_thrs) {
    thr.join();
  }
  // sleep and loop waiting for the set to be filled with the values from the reader threads
  bool done {false};
  while (!done) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::lock_guard<std::mutex> lk(mut);
    if (s.size() == tot) {
      wq.request_stop(); // tell the readers it's done
      done = true;
    }
  }

  // join readers; since wait queue is stopped they should all join immediately
  for (auto& thr : rd_thrs) {
    thr.join();
  }
  REQUIRE (wq.empty());
  REQUIRE (wq.stop_requested());
  // check set to make sure all entries are present
  int idx {0};
  for (const auto& e : s) {
    REQUIRE (e.first == idx);
    REQUIRE (e.second == val);
    ++idx;
  }
  return (s.size() == tot);
}

// non threaded test, multiple container types, multiple element types

TEMPLATE_TEST_CASE ( "Non-threaded wait_queue test", "[wait_queue] [non_threaded]",
         (std::deque<int>), (std::deque<double>), (std::deque<short>), (std::deque<std::string>),
         (nonstd::ring_span<int>), (nonstd::ring_span<double>), (nonstd::ring_span<short>), (nonstd::ring_span<std::string>),
         (jm::circular_buffer<int, N>), (jm::circular_buffer<double, N>),
         (jm::circular_buffer<short, N>), (jm::circular_buffer<std::string, N>) ) {

  using val_type = typename TestType::value_type;
  val_type val1;
  val_type val2;
  if constexpr (std::is_arithmetic_v<val_type>) {
    val1 = 42;
    val2 = 43;
  }
  else { // assume std::string value type in container - generalize as needed
    val1 = "Howzit going, bro!";
    val2 = "It's hanging, bro!";
  }

  auto wq = create_wait_queue( static_cast<const TestType*>(nullptr) );
  non_threaded_push_test(wq, val1, N);
  if constexpr (std::is_arithmetic_v<val_type>) {
    non_threaded_arithmetic_test(wq, N);
  }
}

/*
*/

SCENARIO ( "Non-threaded wait_queue test, testing copy construction without move construction",
           "[wait_queue] [no_move]" ) {

  struct Foo {
    Foo() = delete;
    Foo(double x) : doobie(x) { }
    Foo(const Foo&) = default;
    Foo(Foo&&) = delete;
    Foo& operator=(const Foo&) = default;
    Foo& operator=(Foo&&) = delete;

    double doobie;

    bool operator==(const Foo& rhs) const { return doobie == rhs.doobie; }
  };

  chops::wait_queue<Foo> wq;
  non_threaded_push_test(wq, Foo(42.0), N);
}

SCENARIO ( "Non-threaded wait_queue test, testing move construction without copy construction",
           "[wait_queue] [no_copy]" ) {

  struct Bar {
    Bar() = delete;
    Bar(double x) : doobie(x) { }
    Bar(const Bar&) = delete;
    Bar(Bar&&) = default;
    Bar& operator=(const Bar&) = delete;
    Bar& operator=(Bar&&) = default;

    double doobie;

    bool operator==(const Bar& rhs) const { return doobie == rhs.doobie; }
  };

  chops::wait_queue<Bar> wq;
  wq.push(Bar(42.0));
  wq.push(Bar(52.0));
  REQUIRE (wq.size() == 2);
  auto ret1 { wq.try_pop() };
  REQUIRE (*ret1 == Bar(42.0));
  auto ret2 { wq.try_pop() };
  REQUIRE (*ret2 == Bar(52.0));
  REQUIRE (wq.empty());
}

SCENARIO ( "Non-threaded wait_queue test, testing complex constructor and emplacement",
           "[wait_queue] [complex_type] [deque]" ) {

  struct Band {
    using engagement_type = std::vector<std::vector<std::string> >;
    Band() = delete;
    Band(double x, const std::string& bros) : doobie(x), brothers(bros), engagements() {
      engagements = { {"Seattle"s, "Portland"s, "Boise"s}, {"Detroit"s, "Cleveland"s},
                      {"London"s, "Liverpool"s, "Leeds"s, "Manchester"s} };
    }
    Band(const Band&) = delete;
    Band(Band&&) = default;
    Band& operator=(const Band&) = delete;
    Band& operator=(Band&&) = delete;
    double doobie;
    std::string brothers;
    engagement_type engagements;

    void set_engagements(const engagement_type& engs) { engagements = engs; }
  };

  chops::wait_queue<Band> wq;
  REQUIRE (wq.size() == 0);
  wq.push(Band{42.0, "happy"s});
  wq.emplace_push(44.0, "sad"s);

  Band b3 { 46.0, "not sure"s };
  Band::engagement_type e { {"Coffee 1"s, "Coffee 2"s}, {"Street corner"s} };
  b3.set_engagements(e);
  wq.push(std::move(b3));

  REQUIRE_FALSE (wq.empty());
  REQUIRE (wq.size() == 3);

  auto val1 { wq.try_pop() };
  auto val2 { wq.try_pop() };
  auto val3 { wq.try_pop() };
  REQUIRE ((*val1).doobie == 42.0);
  REQUIRE ((*val1).brothers == "happy"s);
  REQUIRE ((*val2).doobie == 44.0);
  REQUIRE ((*val2).brothers == "sad"s);
  REQUIRE ((*val2).engagements[0][0] == "Seattle"s);
  REQUIRE ((*val2).engagements[0][1] == "Portland"s);
  REQUIRE ((*val2).engagements[0][2] == "Boise"s);
  REQUIRE ((*val2).engagements[2][0] == "London"s);
  REQUIRE ((*val2).engagements[2][3] == "Manchester"s);
  REQUIRE ((*val3).engagements[0][0] == "Coffee 1"s);
  REQUIRE ((*val3).engagements[1][0] == "Street corner"s);
  REQUIRE (wq.empty());
}

using vv_float = std::vector<std::vector<float>>;
using vv_wq = chops::wait_queue<vv_float>;

const vv_float data1 { { 42.0f, 43.0f }, { 63.0f, 66.0f, 69.0f}, { 7.0f } };
const vv_float data2 { { 8.0f }, { 0.0f }, { } };
const vv_float data3 { };

std::size_t vv_push_func(vv_wq& wq, std::size_t cnt) {
  std::size_t n { 0u };
  while (n < cnt) {
    vv_float vv { data1 };
    wq.push(std::move(vv));
    vv = data2;
    wq.push(std::move(vv));
    vv = data3;
    wq.push(std::move(vv));
    vv = data1;
    wq.push(vv);
    vv = data2;
    wq.push(vv);
    vv = data3;
    wq.push(vv);
    ++n;
  }
  return cnt * 6u;
}

std::size_t vv_pop_func(vv_wq& wq, std::size_t exp) {
  std::size_t n { 0u };
  while (n < exp) {
    auto res = wq.wait_and_pop();
    if (!res) { // queue has been stopped
      break;
    }
    REQUIRE (*res == data1);
    ++n;
    res = wq.wait_and_pop();
    REQUIRE (*res == data2);
    ++n;
    res = wq.wait_and_pop();
    REQUIRE (*res == data3);
    ++n;
  }
  return n;
}

TEST_CASE ( "Vector of vector of float, move and copy",
            "[wait_queue] [float] [vector_vector_float]" ) {

  vv_wq wq;
  constexpr std::size_t cnt { 1000u };
  constexpr std::size_t expected { cnt * 6u };
  auto pop_fut = std::async (std::launch::async, vv_pop_func, std::ref(wq), expected);
  auto push_fut = std::async (std::launch::async, vv_push_func, std::ref(wq), cnt);
  auto push_res = push_fut.get();
  auto pop_res = pop_fut.get();
  wq.request_stop();

  REQUIRE (push_res == pop_res);

}

SCENARIO ( "Fixed size ring_span, testing wrap around with int type",
           "[wait_queue] [int] [ring_span_wrap_around]" ) {


  int buf[N];
  chops::wait_queue<int, nonstd::ring_span<int> > wq(buf+0, buf+N);

  constexpr int Answer = 42;
  constexpr int AnswerPlus = 42+5;

  for (int i {0}; i < N; ++i) {
      wq.push(Answer);
  }
  REQUIRE (wq.size() == N);
  wq.apply([Answer] (const int& i) { REQUIRE(i == Answer); } );

  for (int i {0}; i < N; ++i) {
    wq.push(Answer);
  }
  for (int i {0}; i < (N/2); ++i) {
    wq.push(AnswerPlus);
  }
  // the size is full but half match answer and half answer plus, since there's been wrap
  REQUIRE (wq.size() == N);
  // wait_pop should immediately return if the queue is non empty
  for (int i {0}; i < (N/2); ++i) {
    REQUIRE (wq.wait_and_pop() == Answer);
  }
  for (int i {0}; i < (N/2); ++i) {
    REQUIRE (wq.wait_and_pop() == AnswerPlus);
  }
  REQUIRE (wq.empty());
}

SCENARIO ( "Threaded wait queue, deque int",
           "[wait_queue] [threaded] [int] [deque]" ) {

  chops::wait_queue<set_elem<int> > wq;

  SECTION ( "1 reader, 1 writer thread, 100 slice" ) {
    REQUIRE ( threaded_test(wq, 1, 1, 100, 44) );
  }

  SECTION ( "5 reader, 3 writer threads, 1000 slice" ) {
    REQUIRE ( threaded_test(wq, 5, 3, 1000, 1212) );
  }
  SECTION ( "60 reader, 40 writer threads, 5000 slice" ) {
    REQUIRE ( threaded_test(wq, 60, 40, 5000, 5656) );
  }
}

SCENARIO ( "Threaded wait queue, deque string", 
           "[wait_queue] [threaded] [string] [deque]" ) {

  chops::wait_queue<set_elem<std::string> > wq;

  SECTION ( "60 reader, 40 writer threads, 12000 slice" ) {
    REQUIRE ( threaded_test(wq, 60, 40, 12000, "cool, lit, sup"s) );
  }
}

