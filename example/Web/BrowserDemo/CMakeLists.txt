cmake_minimum_required(VERSION 3.31)
project(LVUIBrowserDemo LANGUAGES CXX)

find_package(LVUI 0.1 CONFIG REQUIRED COMPONENTS Native)
find_package(iiGUI 0.6 CONFIG REQUIRED)

lvui_add_web_application(LVUIBrowserDemo
    SOURCE Main.lvui
    INCLUDES BrowserDemoModel.h
    INSTALL_DESTINATION .)
