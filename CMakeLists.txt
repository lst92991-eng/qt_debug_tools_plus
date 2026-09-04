cmake_minimum_required(VERSION 3.20)

project(MCUDebugTool VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_library(mcd_sdk STATIC
    src/sdk/DataFrame.cpp
    src/sdk/DataFrame.h
)
target_include_directories(mcd_sdk PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(mcd_sdk PUBLIC Qt6::Core)

add_executable(mcd_app WIN32
    src/app/main.cpp
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/app/MainWindow.ui
)
target_link_libraries(mcd_app PRIVATE mcd_sdk Qt6::Widgets)

include(CTest)
if(BUILD_TESTING)
    add_executable(mcd_sdk_data_frame_test
        tests/sdk/DataFrameTest.cpp
    )
    target_link_libraries(mcd_sdk_data_frame_test PRIVATE mcd_sdk)
    add_test(NAME sdk_data_frame
        COMMAND "${CMAKE_COMMAND}" -E env
            "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}"
            "$<TARGET_FILE:mcd_sdk_data_frame_test>"
    )
endif()
