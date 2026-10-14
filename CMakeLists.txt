cmake_minimum_required( VERSION 3.21 )

project( GameListEditor LANGUAGES CXX )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_AUTOMOC ON )
set( CMAKE_AUTORCC ON )

if( CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" )
   add_compile_options( -Wall -Wextra -Wshadow -Wconversion )
endif()

set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

# Version from git: "v1.2.3" on a tag, "v1.2.3-5-g7d1a75e" after it, and
# "2026.10.14+7d1a75e" (commit date + hash) while there is no tag at all.
# "-dirty" is appended for uncommitted changes. "unknown" outside a git checkout.
# ponytail: evaluated at configure time only, so an IDE build that never re-runs
# CMake keeps a stale version; re-run the configure step to refresh it.
set( GLE_VERSION "unknown" )
find_package( Git QUIET )
if( GIT_FOUND )
   execute_process( COMMAND "${GIT_EXECUTABLE}" describe --tags --dirty
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE _ver RESULT_VARIABLE _rc
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET )
   if( _rc EQUAL 0 )
      set( GLE_VERSION "${_ver}" )
   else()
      execute_process( COMMAND "${GIT_EXECUTABLE}" describe --always --dirty
         WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
         OUTPUT_VARIABLE _hash RESULT_VARIABLE _rc1
         OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET )
      execute_process( COMMAND "${GIT_EXECUTABLE}" log -1 --format=%cd --date=format:%Y.%m.%d
         WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
         OUTPUT_VARIABLE _date RESULT_VARIABLE _rc2
         OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET )
      if( _rc1 EQUAL 0 AND _rc2 EQUAL 0 )
         set( GLE_VERSION "${_date}+${_hash}" )
      endif()
   endif()
endif()
message( STATUS "GameListEditorQt version: ${GLE_VERSION}" )
# Plain-text copy for packaging tools, so an installer gets exactly the version the exe reports.
file( WRITE "${CMAKE_BINARY_DIR}/version.txt" "${GLE_VERSION}\n" )

# 6.4 is what Ubuntu 24.04 / Mint 22 ships; nothing here needs more.
find_package( Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Xml Network Multimedia MultimediaWidgets )
if( MINGW AND NOT ZLIB_ROOT )
   # Qt's MinGW kit ships zlib in the compiler sysroot, which CMake does not search.
   get_filename_component( _mingw_bin "${CMAKE_CXX_COMPILER}" DIRECTORY )
   set( ZLIB_ROOT "${_mingw_bin}/../x86_64-w64-mingw32" )
endif()
find_package( ZLIB REQUIRED )
find_package( GTest QUIET )
if( NOT GTest_FOUND )
   # No system GTest (typical on Windows): build it from source.
   include( FetchContent )
   FetchContent_Declare( googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE )
   set( INSTALL_GTEST OFF CACHE BOOL "" FORCE )
   set( gtest_force_shared_crt ON CACHE BOOL "" FORCE )
   FetchContent_MakeAvailable( googletest )
endif()

include( GoogleTest )

add_library( gle_core STATIC
   src/core/Game.cpp
   src/core/Gamelist.cpp
   src/core/GamelistModel.cpp
   src/core/NameEdit.cpp
   src/core/Resources.cpp
)
target_include_directories( gle_core PUBLIC src/core )
target_link_libraries( gle_core PUBLIC Qt6::Core Qt6::Gui Qt6::Xml ZLIB::ZLIB )

# screenscraper.fr developer credentials (API allowance, not the user's login).
# Deliberately not in the source tree: official release builds inject them from
# CI secrets, everyone else registers their own or builds without (README).
set( GLE_SS_DEVID "$ENV{GLE_SS_DEVID}" CACHE STRING "screenscraper.fr devid" )
set( GLE_SS_DEVPWD "$ENV{GLE_SS_DEVPWD}" CACHE STRING "screenscraper.fr devpassword" )
# Escape for a C string literal; configure_file does no escaping of its own.
foreach( var GLE_SS_DEVID GLE_SS_DEVPWD )
   string( REPLACE "\\" "\\\\" ${var}_C "${${var}}" )
   string( REPLACE "\"" "\\\"" ${var}_C "${${var}_C}" )
endforeach()
configure_file( src/scraper/DevCredentials.h.in generated/DevCredentials.h @ONLY )

add_library( gle_scraper STATIC
   src/scraper/Scraper.cpp
)
target_include_directories( gle_scraper PUBLIC src/scraper PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated" )
target_link_libraries( gle_scraper PUBLIC Qt6::Core Qt6::Network gle_core )

add_library( gle_ui STATIC
   src/ui/GameEditPanel.cpp
   src/ui/ScrapePanel.cpp
   src/ui/F_Main.cpp
   src/ui/dialogs/F_About.cpp
   src/ui/dialogs/F_AddRoms.cpp
   src/ui/dialogs/F_AdvNameEditor.cpp
   src/ui/dialogs/F_ConfigureNetwork.cpp
   src/ui/dialogs/F_Help.cpp
   src/ui/dialogs/F_LinkMedia.cpp
   src/ui/dialogs/F_MoreInfos.cpp
)
target_include_directories( gle_ui PUBLIC src/ui src/ui/dialogs )
target_link_libraries( gle_ui PUBLIC Qt6::Widgets Qt6::Multimedia Qt6::MultimediaWidgets gle_core gle_scraper )

add_executable( GameListEditor WIN32
   src/main.cpp
   Resources/assets.qrc
)
target_link_libraries( GameListEditor PRIVATE gle_ui )
target_compile_definitions( GameListEditor PRIVATE GLE_VERSION="${GLE_VERSION}" )

add_executable( tests
   tests/main.cpp
   tests/tst_dialogs.cpp
   tests/tst_f_advnameeditor.cpp
   tests/tst_f_main.cpp
   tests/tst_gameeditpanel.cpp
   tests/tst_game.cpp
   tests/tst_gamelist.cpp
   tests/tst_gamelistmodel.cpp
   tests/tst_resources.cpp
   tests/tst_scraper.cpp
   tests/tst_scrapepanel.cpp
   Resources/assets.qrc
)
target_include_directories( tests PRIVATE tests )
target_link_libraries( tests PRIVATE gle_core gle_scraper gle_ui GTest::gtest )

enable_testing()
gtest_discover_tests( tests )

# Packaging: `cpack -G DEB` (see build.sh). Test binaries are not installed.
install( TARGETS GameListEditor RUNTIME DESTINATION bin )
install( FILES Resources/GameListEditor.desktop DESTINATION share/applications )
install( FILES Resources/Icons/app.png DESTINATION share/pixmaps RENAME GameListEditor.png )
# The app is GPLv3; Qt, FFmpeg and libstdc++ come from the distro's own packages (with their own
# copyright files), so unlike the Windows installer no third-party licence texts are shipped.
install( FILES LICENSE DESTINATION share/doc/gamelisteditor RENAME copyright )

set( CPACK_GENERATOR DEB )
set( CPACK_PACKAGE_CONTACT "Mike Bergmann <Mike@mdb977.de>" )
set( CPACK_PACKAGE_DESCRIPTION_SUMMARY "Editor for EmulationStation gamelist.xml files" )
# Same version the exe reports. A .deb version must start with a digit: drop git's "v" prefix,
# and fall back for "unknown" (no git).
string( REGEX REPLACE "^v" "" CPACK_PACKAGE_VERSION "${GLE_VERSION}" )
if( NOT CPACK_PACKAGE_VERSION MATCHES "^[0-9]" )
   set( CPACK_PACKAGE_VERSION "0.0.0" )
endif()
set( CPACK_DEBIAN_PACKAGE_SECTION utils )
# Derives the Qt6/zlib runtime dependencies from the built binary (needs dpkg-dev).
set( CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON )
set( CPACK_DEBIAN_FILE_NAME DEB-DEFAULT )
include( CPack )
