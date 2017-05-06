cmake_minimum_required(VERSION 3.4)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

find_package (Threads REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(CURL REQUIRED)

add_subdirectory(src)
