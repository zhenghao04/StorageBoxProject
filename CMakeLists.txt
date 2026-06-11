cmake_minimum_required(VERSION 3.20)

project(StorageBoxLauncher VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(APP_SOURCES
    src/main.cpp
    src/StorageBoxApp.cpp
    src/StorageBoxApp.h
    assets/resources.qrc
)

if (WIN32)
    list(APPEND APP_SOURCES app.rc)
endif()

add_executable(StorageBoxLauncher WIN32
    ${APP_SOURCES}
)

target_link_libraries(StorageBoxLauncher PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

if (MSVC)
    target_compile_options(StorageBoxLauncher PRIVATE /utf-8)
else()
    target_compile_options(StorageBoxLauncher PRIVATE -finput-charset=UTF-8)
endif()

if (WIN32)
    target_link_libraries(StorageBoxLauncher PRIVATE shell32)
endif()
