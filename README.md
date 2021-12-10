# example-poll-with-eventfd
Simple example demonstrating unidirectional inter-thread communication with eventfd and POSIX poll.

Two event file descriptors are created.
They are used by two additionally spawned  threads for a unidirectional communication with the main thread.
Bidirectional communication is also possible with event file descriptors, but not demonstrated here.
Main thread polls periodically event file descriptors for the POLLIN (read) event.
When the POLLIN event is available on any event file descriptor being polled, 
the main thread reads the event and can trigger some operation based on the received event
(not POLLIN event, but the event/number actually sent from the additional threads).


## Build

```bash
cmake -S . -B build
cmake --build build
```
