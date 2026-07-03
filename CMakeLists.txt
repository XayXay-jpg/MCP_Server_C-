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
add_executable(mcp_manager WIN32 main_gui.cpp ${MCP_SOURCES} ${GUI_SOURCES})

target_include_directories(mcp_manager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mcp_manager PRIVATE 
    nlohmann_json::nlohmann_json 
    httplib::httplib
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    ${wxWidgets_LIBRARIES}
)

# ==========================================
# Install & CPack Configuration
# ==========================================
include(GNUInstallDirs)

# Install executable
install(TARGETS mcp_manager
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)

# Install icons (for Linux)
install(DIRECTORY icons DESTINATION ${CMAKE_INSTALL_DATADIR}/mcp_server_manager)

# Linux desktop file (if building on Linux)
if(UNIX AND NOT APPLE)
    install(FILES linux/mcp-server-manager.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES icons/logo.png DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps RENAME mcp-server-manager.png)
endif()

# CPack settings
set(CPACK_PACKAGE_NAME "mcp-server-manager")
set(CPACK_PACKAGE_VENDOR "MCP Team")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MCP Server Manager C++ Application")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "mcp-server-manager")

# Generator settings
if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_INSTDIR_REGKEY "Software\\\\MCP_Server_Manager")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set_target_properties(mcp_manager PROPERTIES MACOSX_BUNDLE TRUE)
elseif(UNIX)
    set(CPACK_GENERATOR "DEB")
    # Using generic depends, on Ubuntu usually libwxgtk3.2-1t64 or similar, libssl3 or libssl1.1
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libwxgtk3.2-1t64 | libwxgtk3.0-gtk3-0v5, libssl3 | libssl1.1")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Developer")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
endif()

include(CPack)
