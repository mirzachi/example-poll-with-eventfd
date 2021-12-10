cmake_minimum_required( VERSION 3.16 )
project( eventfd_poll )

set( CMAKE_CXX_STANDARD 17 )

# include Threads module (prefer pthread if multiple libs available)
set( CMAKE_THREAD_PREFER_PTHREAD TRUE )
find_package( Threads REQUIRED )

add_executable( eventfd_poll main.cpp )
target_link_libraries(eventfd_poll PRIVATE Threads::Threads)
