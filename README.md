# nanosock
The only minimal and sane socket wrapper for C++ in existence

## Why?

Because all existing network and socket libraries for C++ are absolute disasters:

* Bloated "frameworks" without proper separation of concerns.
* Undocumented for sane use-cases.
* Full of untested functionality and corner cases. (Looking at you, ASIO blocking operations.)
* Full of insanely broken semantics for working with streams of data.
* Important first-class features like read timeout support are usually an afterthought.

## What is this?

This is a thoughtful, but minimal header file for setting up client sockets, making requests and sanely reading the results.

Included is an HTTP client implementation, with pipelining and multi-stream support.

Caveats:

* Tested only for Linux.
* No server socket support yet, only sockets for making requests and reading data. (To be done if there's demand.)
* Blocking, synchronous operations _only_.

## How to use?

Just include `nanosock.h` or `http_request.h`.

`nanosock.h` are the socket building blocks.
`http_request.h` is an implementation of a state machine for making HTTP requests and reading responses based on `nanosock.h`.

### nanosock.h

See the included `tools/example.cc` for usage; but briefly, there are only three classes here:

* `Socket(host, port, timeout)` - socket wrapper for reading and writing; no buffering.
* `Buffer` - a buffer that can read chunks of data from a socket.
* `Reader<MARKER>(terminator)` - a wrapper around a `Buffer` that reads data until the terminator condition is encountered. Data is passed to a user-defined callback function.

`MARKER` is a template parameter that should be a terminator condition.
Three are provided out of the box:

`Marker(std::string)` - read data until the provided string is encountered.

`Count(size_t)` - read data in fixed-size chunks of this size.

`template <typename... MARKER> AnyOf<MARKER...>` - read data until one of the provided terminator conditions fires.

### http_request.h

See the included `tools/get.cc` for usage. There is one class for implementing the HTTP request state machine, and one class for multiplexing concurrent HTTP requests.

* `Request(host, port, timeout)`
* `Mux`

## Public API:

###### `nano::Socket(const std::string& host, unsigned int port, unsigned int timeout=0)`

Connects a socket to a host and port. The timeout value is in milliseconds (so 3 seconds is 3000), the default value of 0 means 'do not enable a timeout'.

`void Socket::send(const std::string& s)`

Writes data to a socket.

###### `nano::Buffer()`

Creates a buffer.

###### `nano::Reader<nano::Marker>(const std::string& terminator)`
###### `nano::Reader<nano::Count>(size_t)`
###### `nano::Reader<nano::AnyOf<nano::Marker, nano::Count, ...>>(const std::string, size_t, ...)`

Creates a reader object for messages with the given end-of-message marker.

`bool Reader::operator()(BUFF& buff, SOCK& sock, FUNC func)`

Reads from the socket until the end-of-message marker is encountered.
The data read, up to and including the end-of-message marker itself, is passed to `func` as a `const std::string&`.

Returns `true` if the end-of-message marker was read.

Returns `false` if the end-of-message marker was not read with all the available data. `operator()` should be called again to fetch the rest of the message.

When `false` is returned, `func` is called with the available data even though the message is incomplete.

Throws a `nano::EndOfSocket` exception when attempting to read from a closed socket.

###### `nano::http::Request(const std::string& host, unsigned int port, unsigned int timeout=0)`

Creates an object for making HTTP requests and receiving responses. This object will create `Socket`, `Buffer` and `Reader` objects as necessary. Connection will be established upon creation of the `Request`.

`void nano::http::Request::send(const std::string& method, const std::string& path, const std::string& body)`

Sends an HTTP request to the connected server. The `Request` must not be in the process of reading a response.

`bool nano::http::Request::transfer(auto& responder, bool blocking = true)`

Read a response. `responder` is a user-provided object to recieve response messages, with the following required methods:

`void version(const std::string&)`

`void code(const std::string&)`

`void header(const std::string&, const std::string&)`

`void body(const std::string&)`

One body payload may be passed as several calls of `body()`.

Returns `true` if all of the message has been read and the `Request` object is ready for `send()`-ing the next request.

`blocking` is used for implementing the stream multiplexor; see below.

###### `nano::Mux<nano::http::Request>()`

A multiplexor object for handling several concurrent HTTP streams in one thread.

`nano::http::Request nano::Mux::add(const std::string& host, unsigned int port, unsigned int timeout=0)`

Creates a `Request` object and adds it to the multiplexor. Returns the just created object.

`void nano::Mux::wait(FUNC func, unsigned int timeout)`

Waits for available data for any of the multiplexed `Request` objects and calls `func` if data is available.
`timeout` is how many milliseconds to wait for data.
Throws a `nano::Timeout` exception on timeout.

`func` is called with two arguments: `Request& req` and `bool blocking`; the first argument is the triggered `Request` object, and the second argument must be transparently passed to `Request::transfer()`.

N.B. Theoretically, streams with other protocols than just HTTP can be multiplexed. The underlying objects must provide `Socket& socket()` and `bool drained()` methods.

