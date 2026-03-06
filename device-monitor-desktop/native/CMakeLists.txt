cmake_minimum_required(VERSION 3.16)
project(DeviceMonitorDesktop VERSION 3.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Charts
)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/networkscanner.cpp
    src/devicemodel.cpp
    src/alertmanager.cpp
    src/overviewpage.cpp
    src/devicespage.cpp
    src/alertspage.cpp
    src/toolspage.cpp
)

set(HEADERS
    src/mainwindow.h
    src/networkscanner.h
    src/devicemodel.h
    src/alertmanager.h
    src/overviewpage.h
    src/devicespage.h
    src/alertspage.h
    src/toolspage.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Charts
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
