cmake_minimum_required(VERSION 3.4.1)

add_subdirectory(lib/curve25519)
add_subdirectory(lib/ed25519)
add_subdirectory(lib/playfair)
add_subdirectory(lib/plist)
add_subdirectory(lib)
add_subdirectory(renderers)

add_executable( rpiplay airplay_server.cpp)
target_link_libraries ( rpiplay renderers play-lib ed25519 )
