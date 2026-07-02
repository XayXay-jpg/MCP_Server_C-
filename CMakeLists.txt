cmake_minimum_required(VERSION 3.14)
project(mcp_server_manager CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

FetchContent_Declare(httplib URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.18.3.tar.gz)
FetchContent_MakeAvailable(httplib)

find_package(Threads REQUIRED)
find_package(wxWidgets COMPONENTS core base REQUIRED)
include(${wxWidgets_USE_FILE})

file(GLOB MCP_SOURCES "mcp/*.cpp")
file(GLOB GUI_SOURCES "gui/*.cpp")

find_package(OpenSSL REQUIRED)

add_executable(mcp_manager main_gui.cpp ${MCP_SOURCES} ${GUI_SOURCES})

target_include_directories(mcp_manager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mcp_manager PRIVATE 
    nlohmann_json::nlohmann_json 
    httplib::httplib
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    ${wxWidgets_LIBRARIES}
)
