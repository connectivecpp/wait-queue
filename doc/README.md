# Wait Queue Detailed Overview

Multiple writer and reader threads can access a `wait_queue` simultaneously. When a value is pushed on the queue by a writer thread, only one reader thread will be notified to consume the value.

A graceful shutdown can be performed using the `request_stop` method (modeled on the C++ 20 `request_stop` from `std::stop_source`). This allows waiting reader threads to be notified for shutdown. Alternatively a `std::stop_token` can be passed in to the `wait_queue` constructor, allowing shutdown from outside of the `wait_queue` object.

Once `request_stop` has been invoked (either through `wait_queue` or from an external `std::stop_source`), subsequent pushes will not add any elements to the queue and the `push` methods will return `false`.

`wait_queue` uses C++ standard library concurrency facilities (e.g. `std::mutex`, `std::condition_variable_any`) in its implementation. It is not a lock-free queue, but it has been designed to be used in memory constrained environments or where deterministic performance is needed. In particular, `wait_queue`:

- Has been tested with Martin Moene's `ring_span` library for the internal container. A `ring_span` is traditionally known as a "ring buffer" or "circular buffer". This implies that the `wait_queue` can be used in environments where dynamic memory management (heap) is not allowed or is problematic. In particular, no heap memory is directly allocated within the `wait_queue` object.

- Does not throw or catch exceptions anywhere in its code base. Elements passed through the queue may throw exceptions, which must be handled at an application level. Exceptions may be thrown by C++ std library concurrency calls (`std::mutex` locks, etc), although this usually indicates an application design issue or issues at the operating system level.

- If the C++ std library concurrency calls become `noexcept` (instead of throwing an exception), every `wait_queue` method will become `noexcept` or conditionally `noexcept` (depending on the type of the data passed through the `wait_queue`).

The only requirement on the type passed through a `wait_queue` is that it supports either copy construction or move construction. In particular, a default constructor is not required (this is enabled by using `std::optional`, which does not require a default constructor).

The implementation is adapted from the book Concurrency in Action, Practical Multithreading, by Anthony Williams (2012 edition). The core logic in this library is the same as provided by Anthony in his book, but the API has changed and additional features added. The name of the utility class template in Anthony's book is `threadsafe_queue`.

The `push` methods return a `bool` to denote whether a value was succesfully queued or whether a shutdown was requested. The `pop` methods return a `std::optional` value. For `wait_and_pop`, if the value is not present it means a shutdown was requested. For `try_pop`, if the value is not present it means either the queue was empty at that moment, or that a shutdown was requested.

Each method is fully documented in the class documentation. In addition, a small amount of example code is present in the file level documentation.

