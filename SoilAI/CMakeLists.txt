cmake_minimum_required(VERSION 3.16)
project(SoilAI VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ------------------------------------------------------------------
# Qt6 modules required by the project
# ------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Charts
    Sql
    SerialPort
    PrintSupport
)

# ------------------------------------------------------------------
# SQLite3 (used directly by DatabaseManager in addition to Qt::Sql)
# ------------------------------------------------------------------
find_package(SQLite3 REQUIRED)

# ------------------------------------------------------------------
# Source / header collection
# ------------------------------------------------------------------
file(GLOB_RECURSE PROJECT_SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)
file(GLOB_RECURSE PROJECT_HEADERS
    "${CMAKE_SOURCE_DIR}/include/*.h"
)

add_executable(SoilAI
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
)

target_include_directories(SoilAI PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/core
    ${CMAKE_SOURCE_DIR}/include/db
    ${CMAKE_SOURCE_DIR}/include/ui
    ${CMAKE_SOURCE_DIR}/include/sensors
    ${CMAKE_SOURCE_DIR}/include/ai
    ${CMAKE_SOURCE_DIR}/include/reports
)

target_link_libraries(SoilAI PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Charts
    Qt6::Sql
    Qt6::SerialPort
    Qt6::PrintSupport
    SQLite::SQLite3
)

# Copy resources (icons, qss themes, model coefficient files) next to the binary
file(COPY ${CMAKE_SOURCE_DIR}/resources DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
