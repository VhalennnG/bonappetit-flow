cmake_minimum_required(VERSION 3.12)
project(bonappetit_flow_backend CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add Homebrew prefix path for Apple Silicon macOS
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")

# Find Drogon
find_package(Drogon REQUIRED)

# Include paths
include_directories(
    ${PROJECT_SOURCE_DIR}/src
    /opt/homebrew/include
)

# Server target
add_executable(be_server
    src/main.cpp
    src/models/State.cpp
    src/controllers/HealthController.cpp
    src/controllers/RoomController.cpp
    src/controllers/OrderController.cpp
    src/queue/WorkerPool.cpp
)

target_link_libraries(be_server PRIVATE Drogon::Drogon)

# WorkerPool unit test target
add_executable(WorkerPool_test
    src/queue/WorkerPool.cpp
    src/queue/WorkerPool_test.cpp
)

target_link_libraries(WorkerPool_test PRIVATE pthread)
