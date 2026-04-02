cmake_minimum_required(VERSION 3.16)
project(NSQCuT VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    WebEngineWidgets
    WebChannel
    Network
    Concurrent
)

find_package(QuaZip-Qt6 REQUIRED)

set(SOURCES
    src/main.cpp
    src/backend.cpp
    src/addonmanager.cpp
    src/settings.cpp
    src/gamelauncher.cpp
)

set(HEADERS
    src/backend.h
    src/addonmanager.h
    src/settings.h
    src/gamelauncher.h
)

qt_add_executable(NSQCuT
    ${SOURCES}
    ${HEADERS}
)

qt_add_resources(NSQCuT "resources"
    PREFIX "/"
    FILES
    resources/index.html
    resources/renderer.js
    resources/style.css
    resources/ipc-bridge.js
)

if(WIN32)
    target_sources(NSQCuT PRIVATE resources/app.rc)
endif()

target_link_libraries(NSQCuT PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebChannel
    Qt6::Network
    Qt6::Concurrent
    QuaZip::QuaZip
)

if(UNIX AND NOT APPLE)
    set_target_properties(NSQCuT PROPERTIES
        OUTPUT_NAME "nsqcut"
    )
elseif(WIN32)
    set_target_properties(NSQCuT PROPERTIES
        OUTPUT_NAME "nsqcut"
        WIN32_EXECUTABLE TRUE
    )
endif()