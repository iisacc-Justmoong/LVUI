cmake_minimum_required(VERSION 3.31)
project(LVUIDeclarativeWeb LANGUAGES CXX)

find_package(LVUI 0.1 CONFIG REQUIRED COMPONENTS Native)
find_package(iiGUI ${LVUI_IIGUI_MIN_VERSION} CONFIG REQUIRED)

lvui_add_web_application(LVUIDeclarativeWeb
    SOURCE Main.lvui
    INCLUDES WebCounterModel.h
    INSTALL_DESTINATION .)
