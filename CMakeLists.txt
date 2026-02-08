cmake_minimum_required(VERSION 3.16)
project(velora_split)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(velora
  src/main.cpp
  src/config.cpp
  src/io.cpp
  src/geo.cpp
  src/time_utils.cpp
  src/heuristic.cpp
  src/report.cpp
  src/stubs.cpp
  src/output_json.cpp
  src/file_utils.cpp
  src/json_serialize.cpp
  src/alns.cpp
)
