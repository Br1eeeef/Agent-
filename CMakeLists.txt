cmake_minimum_required(VERSION 3.16)
project(agent_memory_db VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(agent_memory_core
    src/Memory.cpp
    src/MemoryHashTable.cpp
    src/LruCache.cpp
    src/JsonStorage.cpp
    src/MemoryManager.cpp
)
target_include_directories(agent_memory_core PUBLIC include)

add_executable(agent_memory_demo app/main.cpp)
target_link_libraries(agent_memory_demo PRIVATE agent_memory_core)

enable_testing()
add_executable(agent_memory_tests tests/test_main.cpp)
target_link_libraries(agent_memory_tests PRIVATE agent_memory_core)
add_test(NAME agent_memory_tests COMMAND agent_memory_tests)

