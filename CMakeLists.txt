cmake_minimum_required(VERSION 3.21)
project(QBrowse VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network NetworkAuth Sql Svg WebEngineCore WebEngineWidgets)

qt_add_executable(qbrowse
    src/main.cpp
    src/AppPaths.cpp
    src/SettingsManager.cpp
    src/BookmarkStore.cpp
    src/HistoryStore.cpp
    src/DownloadManager.cpp
    src/RequestInterceptor.cpp
    src/BrowserView.cpp
    src/ThemeManager.cpp
    src/ImportManager.cpp
    src/OAuthAccountService.cpp
    src/MozillaAccountService.cpp
    src/GoogleAccountService.cpp
    src/StoreInstaller.cpp
    src/Dialogs.cpp
    src/MainWindow.cpp
    resources/resources.qrc
)

target_include_directories(qbrowse PRIVATE src)

target_link_libraries(qbrowse PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::NetworkAuth
    Qt6::Sql Qt6::Svg Qt6::WebEngineCore Qt6::WebEngineWidgets
)

if(UNIX AND NOT APPLE)
    install(TARGETS qbrowse RUNTIME DESTINATION bin)
    install(FILES packaging/qbrowse.desktop DESTINATION share/applications)
    install(FILES resources/logo/qbrowse-logo.svg DESTINATION share/icons/hicolor/scalable/apps RENAME qbrowse.svg)
endif()
