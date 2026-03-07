cmake_minimum_required(VERSION 3.20)
project(Transparency VERSION 3.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows subsystem
if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows -municode")
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/TabOverview.cpp
    src/TabDevices.cpp
    src/TabAlerts.cpp
    src/TabTools.cpp
    src/TabPrivacy.cpp
    src/TabLedger.cpp
    src/Scanner.cpp
    src/Monitor.cpp
)

set(HEADERS
    src/Resource.h
    src/Models.h
    src/Theme.h
    src/OuiData.h
    src/Scanner.h
    src/Monitor.h
    src/MainWindow.h
    src/TabOverview.h
    src/TabDevices.h
    src/TabAlerts.h
    src/TabTools.h
    src/TabPrivacy.h
    src/TabLedger.h
)

add_executable(Transparency WIN32 ${SOURCES} ${HEADERS})

target_compile_definitions(Transparency PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
    _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
    target_compile_options(Transparency PRIVATE
        /W3
        /MP
        /utf-8
        /Zc:__cplusplus
    )
endif()

target_link_libraries(Transparency PRIVATE
    ws2_32
    iphlpapi
    comctl32
    uxtheme
    dwmapi
    dnsapi
    gdi32
    ole32
    winmm
    shell32
    wlanapi
    ntdll
    comdlg32
)

set_target_properties(Transparency PROPERTIES
    OUTPUT_NAME "Transparency"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

target_include_directories(Transparency PRIVATE src)

# Enable testing
enable_testing()

# Test executable for ScanEngine::AnalyzeAnomalies (we use console subsystem for tests)
add_executable(TransparencyTests src/TestScannerAnomalies.cpp src/Scanner.cpp)
if(MINGW OR CYGWIN)
    # Ensure tests run as console app so output is visible
    set_target_properties(TransparencyTests PROPERTIES LINK_FLAGS "-mconsole -static-libgcc -static-libstdc++")
elseif(MSVC)
    set_target_properties(TransparencyTests PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()
target_compile_definitions(TransparencyTests PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
    _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
    target_compile_options(TransparencyTests PRIVATE
        /W3
        /MP
        /utf-8
        /Zc:__cplusplus
    )
endif()
target_link_libraries(TransparencyTests PRIVATE
    ws2_32
    iphlpapi
    comctl32
    uxtheme
    dwmapi
    dnsapi
    gdi32
    ole32
    winmm
    shell32
    wlanapi
    ntdll
    comdlg32
)
target_include_directories(TransparencyTests PRIVATE src)
if(MSVC)
    target_compile_options(TransparencyTests PRIVATE
        /wd4100
        /wd4189
    )
    set_property(TARGET TransparencyTests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_test(NAME TransparencyTests COMMAND TransparencyTests)

# MSVC specific settings
if(MSVC)
    target_compile_options(Transparency PRIVATE
        /wd4100  # unreferenced formal parameter
        /wd4189  # local variable initialized but not referenced
    )
    set_property(TARGET Transparency PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
