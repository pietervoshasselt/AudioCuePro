cmake_minimum_required(VERSION 3.16)

project(AudioCuePro LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.10 COMPONENTS
    Widgets
    Gui
    Multimedia
    MultimediaWidgets
    Concurrent
    Network
    REQUIRED
)

qt_standard_project_setup()

qt_add_executable(AudioCuePro
    main.cpp
    mainwindow.cpp
    sfxlibrarywidget.cpp
    trackwidget.cpp
    waveformview.cpp
    livemodewindow.cpp          # ← NEW

    mainwindow.h
    trackwidget.h
    waveformview.h
    livemodewindow.h            # ← NEW
	spotifyclient.cpp
    spotifyclient.h
	spotifyauthmanager.cpp
	spotifyauthmanager.h
	spotifymodule.cpp
    spotifymodule.h

)


target_link_libraries(AudioCuePro PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Concurrent
    Qt6::Network
)

# Make the executable output into the build root
set_target_properties(AudioCuePro PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
