# Chat client & server
- Handles multiple clients at once.
- Uses **Unix domain sockets** which can be easily replaced by any other socket type.

### Dependency Installation
- SQLite3 library for C++:
```sh
$ sudo apt-get install libsqlite3-dev
```
### Compilation
```sh
$ g++ --std=c++11 server.cpp -lsqlite3 -o server
$ g++ client.cpp -o client
```
