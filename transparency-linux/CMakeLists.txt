cmake_minimum_required(VERSION 3.16)
project(Transparency VERSION 3.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
find_package(Threads REQUIRED)

add_executable(transparency
    src/main.cpp
    src/scanner.cpp
    src/app.cpp
)

target_include_directories(transparency PRIVATE
    ${GTK3_INCLUDE_DIRS}
    src/
)

target_link_libraries(transparency PRIVATE
    ${GTK3_LIBRARIES}
    Threads::Threads
)

target_compile_options(transparency PRIVATE ${GTK3_CFLAGS_OTHER})

install(TARGETS transparency DESTINATION bin)
