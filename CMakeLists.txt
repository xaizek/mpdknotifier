project(mpdknotifier)
find_package(KDE4 REQUIRED)
include(KDE4Defaults)
include_directories(${KDE4_INCLUDES})
set(mpdknotifier_SRCS src/main.cpp src/notifier.cpp)
kde4_add_executable(mpdknotifier ${mpdknotifier_SRCS})
target_link_libraries(mpdknotifier ${KDE4_KDEUI_LIBS} ${QT_QTNETWORK_LIBRARY})
install(FILES mpdknotifier.notifyrc DESTINATION ${DATA_INSTALL_DIR}/mpdknotifier)
install(FILES nocover.jpg DESTINATION ${DATA_INSTALL_DIR}/mpdknotifier)
install(TARGETS mpdknotifier ${INSTALL_TARGETS_DEFAULT_ARGS})
