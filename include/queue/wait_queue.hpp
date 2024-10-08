/** @mainpage Wait Queue, a Multi-Writer / Multi-Reader (MPMC) Thread-Safe Queue
 *
 * ## Overview
 *
 * This class template allows transferring data between threads with queue semantics 
 * (push, pop), using C++ std library general facilities (mutex, condition variable). 
 * An internal container is managed within an object of this class template. 
 *
 * Multiple writer and reader threads can access a @c wait_queue object simultaneously. 
 * When a value is pushed on the queue by a writer thread, only one reader thread will be 
 * notified to consume the value.
 *
 * One of the template parameters is the container type, allowing customization 
 * for specific use cases (see below for additional details). The default container
 * type is @c std::deque.
 *
 * A graceful shutdown can be requested using the @c request_stop method (modeled on the 
 * C++ 20 @c request_stop from @c std::stop_source). This allows waiting reader threads 
 * to be notified for shutdown. Alternatively a @c std::stop_token can be passed in to 
 * the @c wait_queue constructor, allowing shutdown from outside of the @c wait_queue 
 * object.
 *
 * @c wait_queue uses C++ standard library concurrency facilities (e.g. @c std::mutex, 
 * @c std::condition_variable_any) in its implementation. It is not a lock-free queue, 
 * but it has been designed to be used in memory constrained environments or where 
 * deterministic performance is needed. 
 *
 * In particular, @c wait_queue:
 *
 * - Has been tested with Martin Moene's @c ring_span library for the internal container,
 *   as well as Justin Masiulis' @c circular_buffer library. A "ring buffer" or 
 *   "circular buffer" uses a fixed size container and implies that the @c wait_queue can 
 *   be used in environments where dynamic memory management (heap) is not allowed or is 
 *   problematic. In particular, no heap memory will be directly allocated within the 
 *   @c wait_queue object. A @c ring_span is a view on a container object instead of 
 *   directly owning the container, so there are differences in construction and
 *   container management.
 *
 * - Does not throw or catch exceptions anywhere in its code base. If a value being pushed
 *   on to the queue throws an exception, it can be caught by the pushing code (or higher
 *   up in the call chain). Exceptions may be thrown by C++ std library concurrency calls 
 *   (@c std::mutex locks, etc), as specified by the C++ standard, although this usually 
 *   indicates an application design issue or issues at the operating system level.
 *
 * - If the C++ std library concurrency calls become @c noexcept (instead of throwing an 
 *   exception), every @c wait_queue method will become @c noexcept or conditionally 
 *   @c noexcept (depending on the type of the data passed through the @c wait_queue).
 *
 * The only requirement on the type passed through a @c wait_queue is that it supports 
 * either copy construction or move construction. In particular, a default constructor is 
 * not required (this is enabled by using @c std::optional, which does not require a 
 * default constructor).
 *
 * The implementation is adapted from the book Concurrency in Action, Practical 
 * Multithreading, by Anthony Williams (2012 edition). The core logic in this library is 
 * the same as provided by Anthony in his book, but C++ 20 features have been added,
 * the API is significantly changed and additional features added. The name of the utility 
 * class template in Anthony's book is @c threadsafe_queue.
 *
 * ### Additional Details
 *
 * Each method is fully documented in the class documentation. In particular, function
 * arguments, pre-conditions, and return values are all documented.
 *
 * Once @c request_stop has been invoked (either through the @c wait_queue object or 
 * from an external @c std::stop_source), subsequent pushes will not add any elements to 
 * the queue and the @c push methods will return @c false.
 *
 * The @c push methods return a @c bool to denote whether a value was succesfully queued or 
 * whether a shutdown was requested. The @c pop methods return a @c std::optional value. 
 * For the @c wait_and_pop method, if the return value is not present it means a shutdown was 
 * requested. For the @c try_pop method, if the return value is not present it means either 
 * the queue was empty at that moment, or that a shutdown was requested.
 *
 * A @c std::stop_token can be passed in through the constructors, which allows
 * aa external @c std::stop_source to @c request_stop. Alternatively, an
 * internal @c stop_token will be used, allowing the @c wait_queue 
 * @c request_stop method to be used to shutdown @c wait_queue processing.
 *
 * Once a @c request_stop is called (either externally or through the @c wait_queue
 * @c request_stop) all reader threads calling @c wait_and_pop are notified, and an empty 
 * value returned to those threads. Subsequent calls to @c push will return a @c false value.
 *
 * Example usage, default container:
 *
 * @code
 *   chops::wait_queue<int> wq;
 *
 *   // inside writer thread, assume wq passed in by reference
 *   wq.push(42);
 *   ...
 *   // all finished, time to shutdown
 *   wq.request_stop();
 *
 *   // inside reader thread, assume wq passed in by reference
 *   auto rtn_val = wq.wait_and_pop(); // return type is std::optional<int>
 *   if (!rtn_val) { // empty value, request_stop has been called
 *     // time to exit reader thread
 *   }
 *   if (*rtn_val == 42) ...
 * @endcode
 *
 * Example usage with ring buffer (from Martin Moene):
 *
 * @code
 *   const int sz = 20;
 *   int buf[sz];
 *   chops::wait_queue<int, nonstd::ring_span<int> > 
 *         wq { nonstd::ring_span<int> { buf+0, buf+sz } };
 *   // push and pop same as code with default container
 * @endcode
 *
 * The container type must support the following (depending on which 
 * methods are called): default construction, construction with an initial size, 
 * @c push_back (preferably overloaded for both copy and move semantics), 
 * @c emplace_back (with a template parameter pack), @c front, @c pop_front, 
 * @c empty, and @c size. The container must also have a @c size_type
 * defined. Type constraints and concepts are defined for the various
 * type requirements.
 *
 * Iterators on a @c wait_queue are not supported, due to obvious difficulties 
 * with maintaining consistency and integrity. The @c apply method can be used to 
 * access the internal data in a threadsafe manner.
 *
 * Copy and move construction or assignment for the whole queue is
 * disallowed, since the use cases and underlying implications are not clear 
 * for those operations. In particular, the exception implications for 
 * assigning the internal data from one queue to another is messy, and the general 
 * semantics of what it means is not clearly defined. If there is data in one 
 * @c wait_queue that must be copied or moved to another, the @c apply method can 
 * be used or individual @c push and @c pop methods called, even if not as efficient 
 * as an internal copy or move.
 *
 * @note The @c boost @c circular_buffer can be used for the container type. Memory is
 * allocated only once, at container construction time. This may be useful for
 * environments where construction can use dynamic memory but a @c push or @c pop 
 * must not allocate or deallocate memory. If the container type is @c boost 
 * @c circular_buffer then the default constructor for @c wait_queue cannot be used 
 * (since it would result in a container with an empty capacity).
 *
 * Thanks go to Lou Langholtz for adding DBC (Design by Contract) assertions.
 *
 * @authors Cliff Green, Lou Langholtz, Anthony Williams
 *
 * @copyright (c) 2017-2024 by Cliff Green
 *
 * Distributed under the Boost Software License, Version 1.0. 
 * (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef WAIT_QUEUE_HPP_INCLUDED
#define WAIT_QUEUE_HPP_INCLUDED

#include <cassert> // assert
#include <deque>
#include <mutex> // std::scoped_lock, std::mutex
#include <condition_variable>
#include <stop_token> // std::stop_source, std::stop_token
#include <optional>
#include <utility> // std::move, std::move_if_noexcept, std::forward
#include <type_traits> // for requires clauses and noexcept specs
// #include <concepts> 

namespace chops {

// requirements for wait_queue container

template <typename Ctr, typename T>
concept supports_push_back = requires (Ctr ctr, T val) {
  ctr.push_back(val);
};

template <typename Ctr, typename ... Args>
concept supports_emplace_back = requires (Ctr ctr, Args&& ... args) {
  ctr.emplace_back(args ...);
};

template <typename Ctr>
concept supports_empty = requires (Ctr ctr) {
  ctr.empty();
};

template <typename Ctr>
concept supports_pop_front = requires (Ctr ctr) {
  ctr.pop_front();
};

template <typename Ctr>
concept supports_size = requires (Ctr ctr) {
  ctr.size();
};

/**
 * @brief MPMC thread-safe wait queue with shutdown semantics.
 *
 * @tparam T Type of value that will be passed through the queue.
 *
 * @tparam Container Type of container that is used as the underlying data queue.
 *
 * @pre The value type must be either copy constructible or move constructible. It does not
 * have to be both, and in particular a default constructor is not required.
 *
 * @pre The container type must support certain operations depending on which ones are called.
 * The constraints are specified on each particular operation.
 *
 */
template <typename T, typename Container = std::deque<T> >
  requires std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>
class wait_queue {
private:
  mutable std::mutex              m_mut;
  std::optional<std::stop_source> m_stop_src;
  std::stop_token                 m_stop_tok;
  std::condition_variable_any     m_data_cond;
  Container                       m_data_queue;

  using lock_guard = std::scoped_lock<std::mutex>;

public:

  using size_type = typename Container::size_type;
  using value_type = T;

public:

  /**
   * @brief Default construct a @c wait_queue.
   *
   * An internal @c stop_source is used to provide a @c std::stop_token for
   * coordinating shutdown.
   *
   * @note A default constructed @c boost @c circular_buffer cannot do
   * anything, so a different @c wait_queue constructor must be used if
   * instantiated with a @c boost @c circular_buffer.
   *
   * @post @c empty returns @c true.
   * @post @c size returns 0.
   * @post @c stop_requested returns @c false.
   */
  wait_queue()
    requires std::is_default_constructible_v<Container>
    // noexcept(std::is_nothrow_constructible_v<Container>)
      : m_stop_src(std::stop_source{}), m_stop_tok((*m_stop_src).get_token()) 
  {
    assert(empty());
    assert(size() == size_type(0));
    assert(!stop_requested());
  }

  /**
   * @brief Construct a @c wait_queue with an externally provided @c std::stop_token.
   *
   * @param stop_tok A @c std::stop_token which can be used to shutdown @c wait_queue
   * processing.
   *
   * @post @c empty returns @c true.
   * @post @c size returns 0.
   */
  wait_queue(std::stop_token stop_tok)
    requires std::is_default_constructible_v<Container>
    // noexcept(std::is_nothrow_constructible_v<Container, std::stop_token>)
      : m_stop_tok(stop_tok) 
  {
    assert(empty());
    assert(size() == size_type(0));
  }

  /**
   * @brief Construct a @c wait_queue by moving in an already constructed 
   * container.
   *
   * This constructor allows a container view to be used for the @c wait_queue
   * container. Typically a container view is initialized with an underlying
   * object, for example a statically allocated array. This allows @c wait_queue
   * to be used where dynamic memory is not allowed.
   *
   * This constructor also allows arbitrary initialization of the data inside
   * the container before it is managed by the @c wait_queue.
   *
   * An internal @c std::stop_source is used to provide a @c std::stop_token for
   * coordinating shutdown.
   *
   * @param container Container object to be moved from (or copied from if not
   * movable).
   *
   * @post @c empty and @c size match moved (or copied) in container.
   */
  wait_queue(Container&& container)
    requires std::is_move_constructible_v<Container> ||
             std::is_copy_constructible_v<Container>
    // noexcept(std::is_nnthrow_constructible_v<Container, Container&&>)
      : m_stop_src(std::stop_source{}), m_stop_tok((*m_stop_src).get_token()),
	m_data_queue(std::move(container)) 
  {
    // not easily assertible until contracts added to C++
  }

  /**
   * This constructor allows a container view to be used for the @c wait_queue
   * container. It also takes a @c std::stop_token for external shutdown.
   *
   * @param stop_tok A @c std::stop_token which can be used to shutdown @c wait_queue
   * processing.
   *
   * @param container Container object to be moved from (or copied from if not
   * movable).
   *
   * @post @c empty and @c size match moved (or copied) in container.
   */
  wait_queue(std::stop_token stop_tok, Container&& container)
    requires std::is_move_constructible_v<Container> ||
             std::is_copy_constructible_v<Container>
    // noexcept(std::is_nothrow_constructible_v<Container, std::stop_token, Container&&>)
      : m_stop_tok(stop_tok), m_data_queue(std::move(container)) 
  {
  }

  /**
   * @brief Construct a @c wait_queue with an initial size or capacity.
   *
   * Construct the container (or container view) with an initial size of default
   * inserted elements or with an initial capacity, depending on the container type.
   *
   * An internal @c std::stop_source is used to provide a @c std::stop_token for
   * coordinating shutdown.
   *
   * @note This constructor cannot be used with a @c ring_span container type.
   * 
   * @note Using this constructor with a @c boost @c circular_buffer creates a
   * container with the specified capacity, but an initial empty size.
   *
   * @note Using this constructor with most standard library container types 
   * creates a container initialized with default inserted elements.
   *
   * @param sz Capacity or initial size, depending on container type.
   *
   * @post If @c sz is 0 @c empty returns @c true, else behavior depends on container used.
   * @post @c size returns 0 or @c sz depending on container used.
   */
  wait_queue(size_type sz)
    requires std::is_constructible_v<Container, size_type>
    // noexcept(std::is_nothrow_constructible_v<Container, size_type>)
      : m_stop_src(std::stop_source{}), m_stop_tok((*m_stop_src).get_token()),
	m_data_queue(sz) 
  {
    assert((sz != size_type(0)) || empty());
    assert((size() == size_type(0)) || (size() == sz));
  }

  /**
   * @brief Construct a @c wait_queue with an initial size or capacity along
   * with a @c std::stop_token.
   *
   * @param stop_tok A @c std::stop_token which can be used to shutdown @c wait_queue
   * processing.
   *
   * @param sz Capacity or initial size, depending on container type.
   *
   * @post If @c sz is 0 @c empty returns @c true, else behavior depends on container used.
   * @post @c size returns 0 or @c sz depending on container used.
   */
  wait_queue(std::stop_token stop_tok, size_type sz)
    requires std::is_constructible_v<Container, std::stop_token, size_type>
    // noexcept(std::is_nothrow_constructible_v<Container, std::stop_token, size_type>)
      : m_stop_tok((*m_stop_src).get_token()), m_data_queue(sz) 
  {
    assert((sz != size_type(0)) || empty());
    assert((size() == size_type(0)) || (size() == sz));
  }

  // disallow copy or move construction of the entire object
  wait_queue(const wait_queue&) = delete;
  wait_queue(wait_queue&&) = delete;

  // disallow copy or move assigment of the entire object
  wait_queue& operator=(const wait_queue&) = delete;
  wait_queue& operator=(wait_queue&&) = delete;

  // modifying methods

  /**
   * @brief Request the @c wait_queue to stop processing, unless a @c std::stop_token
   * was passed in to a constructor.
   *
   * If a @c std::stop_token was passed into a constructor, a @c request_stop must
   * be performed external to the @c wait_queue and this method has no effect.
   *
   * For an internal @c std::stop_token, all waiting reader threaders will be notified. 
   * Subsequent @c push operations will return @c false.
   *
   * @return @c true if an internal @c stop_source was used (versus a @c std::stop_token
   * passed in to the constructor) and the request returns @c true, @c false if an 
   * external @c std::stop_token was passed in.
   */
  auto request_stop() noexcept 
        -> bool 
  {
    if (m_stop_src) {
      return (*m_stop_src).request_stop();
    }
    return false;
  }

  /**
   * @brief Push a value, by copying, to the @c wait_queue.
   *
   * When a value is pushed, one waiting reader thread (if any) will be 
   * notified that a value has been added.
   *
   * @param val Val to copy into the queue.
   *
   * @return @c true if successful, @c false if the @c wait_queue has been
   * requested to stop.
   *
   * @post If @c true is returned and @c empty is @c false, one of any threads waiting for a
   *   value will be unblocked.
   */
  auto push(const T& val) /* noexcept(std::is_nothrow_copy_constructible_v<T>) */
        -> bool 
    requires supports_push_back<Container, T>

  {
    if (m_stop_tok.stop_requested()) {
      return false;
    }
    lock_guard lk{m_mut};
    m_data_queue.push_back(val);
    m_data_cond.notify_one();
    return true;

  }

  /**
   * @brief Push a value, either by moving or copying, to the @c wait_queue.
   *
   * This method has the same semantics as the other @c push, except that the value will 
   * be moved (if possible) instead of copied.
   *
   * @post If @c true is returned and @c empty is @c false, one of any threads waiting for a
   *   value will be unblocked.
   */
  auto push(T&& val) /* noexcept(std::is_nothrow_move_constructible_v<T>) */ 
        -> bool
    requires supports_push_back<Container, T>

  {
    if (m_stop_tok.stop_requested()) {
      return false;
    }
    lock_guard lk{m_mut};
    m_data_queue.push_back(std::move(val));
    m_data_cond.notify_one();
    return true;

  }

  /**
   * @brief Directly construct an object in the underlying container (using the container's
   * @c emplace_back method) by forwarding the supplied arguments (can be more than one).
   *
   * @param args Arguments to be used in constructing an element at the end of the queue.
   *
   * @note The @c std containers return a reference to the newly constructed element from 
   * @c emplace method calls. @c emplace_push for a @c wait_queue does not follow this 
   * convention and instead has the same return as the @c push methods.
   *
   * @return @c true if successful, @c false if the @c wait_queue is has been requested
   * to stop.
   *
   * @post If @c true is returned and @c empty is @c false, one of any threads waiting for a
   *   value will be unblocked.
   */
  template <typename ... Args>
  auto emplace_push(Args &&... args) /* noexcept(std::is_nothrow_constructible_v<T, Args...>)*/
        -> bool 
    requires supports_emplace_back<Container, Args...>
 
  {
    if (m_stop_tok.stop_requested()) {
      return false;
    }
    lock_guard lk{m_mut};
    m_data_queue.emplace_back(std::forward<Args>(args)...);
    m_data_cond.notify_one();
    return true;
  }

  /**
   * @brief Pop and return a value from the @c wait_queue, blocking and waiting for a writer 
   * thread to push a value if one is not immediately available.
   *
   * If this method is called after a @c wait_queue has been requested to stop, an empty 
   * @c std::optional is returned. If a @c wait_queue needs to be flushed after it is stopped, 
   * @c try_pop should be called instead.
   *
   * @return A value from the @c wait_queue (if non-empty). If the @c std::optional is empty, 
   * the @c wait_queue has been requested to be stopped.
   *
   * @post If a non empty value is returned, until a push function is called, @c size is one
   *   less than before this function was called.
   */
  [[nodiscard]] auto wait_and_pop() /* noexcept(std::is_nothrow_constructible_v<T>) */
        -> std::optional<T> 
      requires supports_empty<Container> && supports_pop_front<Container>

  {
    std::unique_lock<std::mutex> lk{m_mut};
    if (!m_data_cond.wait ( lk, m_stop_tok, [this] { return !m_data_queue.empty(); } )) {
      return std::optional<T> {}; // queue was request to stop, no data available
    }
    assert(!m_data_queue.empty());
#ifndef NDEBUG
    const auto old_size = m_data_queue.size();
#endif
    std::optional<T> val {std::move_if_noexcept(m_data_queue.front())}; // move construct if possible
    m_data_queue.pop_front();
    assert(m_data_queue.size() + 1u == old_size);
    return val;

  }

  /**
   * @brief Pop and return a value from the @c wait_queue if an element is immediately 
   * available, otherwise return an empty @c std::optional.
   *
   * @return A value from the @c wait_queue or an empty @c std::optional if no values are 
   * available in the @c wait_queue or if the @c wait_queue has been requested to be 
   * stopped .
   *
   * @post If a non empty value is returned, until a push function is called, @c size is one
   *   less than before this function was called.
   */
  [[nodiscard]] auto try_pop() /* noexcept(std::is_nothrow_constructible_v<T>) */
        -> std::optional<T> 
      requires supports_empty<Container> && supports_pop_front<Container>
  {
    if (m_stop_tok.stop_requested()) {
      return std::optional<T> {};
    }
    lock_guard lk{m_mut};
    if (m_data_queue.empty()) {
      return std::optional<T> {};
    }
#ifndef NDEBUG
    const auto old_size = m_data_queue.size();
#endif
    std::optional<T> val {std::move_if_noexcept(m_data_queue.front())}; // move construct if possible
    m_data_queue.pop_front();
    assert(m_data_queue.size() + 1u == old_size);
    return val;

  }

  // non-modifying methods

  /**
   * @brief Apply a non-modifying function object to all elements of the queue.
   *
   * The function object is not allowed to modify any of the elements. 
   * The supplied function object is passed a const reference to the element 
   * type.
   *
   * This method can be used when an iteration of the elements is needed,
   * such as to print the elements, or copy them to another container, or 
   * to interrogate values of the elements.
   *
   * @param func Function object to be invoked on each element. The function
   * object should have the signature:
   * @code
   * void (const T&);
   * @endcode
   * where @c T is the type of element in the queue.
   *
   * @note The entire @c wait_queue is locked while @c apply is in process, 
   * so passing in a function object that blocks or takes a lot of processing 
   * time may result in slow performance.
   *
   * @note It is undefined behavior if the function object calls into the 
   * same @c wait_queue since it results in recursive mutex locks.
   */
  template <typename F>
  auto apply(F&& func) const /* noexcept(std::is_nothrow_invocable_v<F&&, const T&>) */
        -> void 
      requires std::is_invocable_v<F, T>

  {
    lock_guard lk{m_mut};
    for (const T& elem : m_data_queue) {
      func(elem);
    }

  }

  /**
   * Query whether a @ request_stop method has been called on 
   * the @c wait_queue.
   *
   * @return @c true if the @c stop_requested has been called.
   */
  [[nodiscard]] auto stop_requested() const noexcept
        -> bool 

  {
    return m_stop_tok.stop_requested();
  }

  /**
   * Query whether the @c wait_queue is empty or not.
   *
   * @return @c true if the @c wait_queue is empty.
   */
  [[nodiscard]] auto empty() const /* noexcept */
        -> bool 
      requires supports_empty<Container>

  {
    lock_guard lk{m_mut};
    return m_data_queue.empty();

  }

  /**
   * Get the number of elements in the @c wait_queue.
   *
   * @return Number of elements in the @c wait_queue.
   */
  [[nodiscard]] auto size() const /* noexcept */
        -> size_type 
      requires supports_size<Container>

  {
    lock_guard lk{m_mut};
    return m_data_queue.size();

  }

};

} // end namespace

#endif

