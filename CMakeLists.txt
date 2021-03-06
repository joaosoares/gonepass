cmake_minimum_required (VERSION 3.0)

# CMake include path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake )

project(gonepass)

SET( PKGDATADIR ${CMAKE_INSTALL_PREFIX}/${DATA_INSTALL_DIR} )

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(JANSSON REQUIRED jansson)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(URIPARSER REQUIRED liburiparser)
pkg_check_modules(OATH REQUIRED liboath)
pkg_check_modules(CHECK REQUIRED check)


# GResource
include(GResource)
gresource(${CMAKE_CURRENT_SOURCE_DIR} gonepassapp.gresource.xml
    ${CMAKE_BINARY_DIR} RESOURCE_FILE)

include_directories (${GTK3_INCLUDE_DIRS} ${JANSSON_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS} ${URIPARSER_INCLUDE_DIRS} ${OATH_INCLUDE_DIRS})
link_directories (${GTK3_LIBRARY_DIRS} ${JANSSON_LIBRARY_DIRS} ${OPENSSL_LIBRARY_DIRS} ${URIPARSER_LIBRARY_DIRS} ${OATH_LIBRARY_DIRS})

set (SOURCES
    appwindow.c
    gonepassapp.c
    item_builder.c
    main.c
    ${RESOURCE_FILE}
)

set (HEADERS
    gonepass.h
)

add_executable(gonepass
    ${SOURCES}
)

target_link_libraries(gonepass
    ${GTK3_LIBRARIES}
    ${JANSSON_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${URIPARSER_LIBRARIES}
    ${OATH_LIBRARIES}
)

# GSettings
include(GSettings)
add_schemas(gonepass ${CMAKE_CURRENT_SOURCE_DIR})

install (TARGETS gonepass DESTINATION bin)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/gonepass.desktop.in
    ${CMAKE_BINARY_DIR}/gonepass.desktop
)

install (
    FILES
        ${CMAKE_BINARY_DIR}/gonepass.desktop
    DESTINATION
        share/applications
)

install (
    FILES
        gonepass-icon.png
    DESTINATION
        share/pixmaps
    RENAME
        gonepass.png
)
