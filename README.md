# nanosock
The only minimal and sane socket wrapper for C++ in existence

## Why?

Because all existing network and socket libraries for C++ are absolute disasters:

* Bloated "frameworks" without proper separation of concerns.
* Undocumented for sane use-cases.
* Full of untested functionality and corner cases. (Looking at you, ASIO blocking operations.)
* Full of insanely broken semantics for working with streams of data.
* Has read timeout support out of the box.

## What is this?

This is an absolute minimal header file for setting up client sockets, making requests and sanely reading the results.

Caveats:

* Tested only for Linux.
* No server socket support yet, only sockets for making requests and reading data. (To be done if there's demand.)
* Blocking, synchronous operations _only_.

## How to use?

Just include `nanosock.h`.

See the included `example.cc` for usage; but briefly, there are only three classes here:

* `Socket(host, port)` - socket wrapper for reading and writing; no buffering.
* `Buffer` - a buffer that can read chunks of data from a socket.
* `Reader(terminator)` - a wrapper around a `Buffer` that reads data until the terminator string is encountered. Data is passed to a user-defined callback function.

## Public API:

`nano::Socket sock(const std::string& host, unsigned int port)`

Connects a socket to a host and port.

`void Socket::send(const std::string& s)`

Writes data to a socket.

`nano::Buffer()`

Creates a buffer.

`nano::Reader reader(const std::string& terminator)`

Creates a reader object for messages with the given end-of-message marker.

`bool Reader::operator()(BUFF& buff, SOCK& sock, FUNC func)`

Reads from the socket until the end-of-message marker is encountered.
The data read, up to and including the end-of-message marker itself, is passed to `func` as a `const std::string&`.

Returns `true` is the end-of-message marker was read.

Returns `false` in case the data was read until the socket was closed.

When `false` is returned, `func` is still called with the remaining data.
