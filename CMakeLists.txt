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

# 6.4 is what Ubuntu 24.04 / Mint 22 ships; nothing here needs more.
find_package( Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Xml Network )
find_package( ZLIB REQUIRED )
find_package( GTest REQUIRED )

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
   src/ui/dialogs/F_AdvNameEditor.cpp
   src/ui/dialogs/F_ConfigureNetwork.cpp
   src/ui/dialogs/F_Help.cpp
   src/ui/dialogs/F_MoreInfos.cpp
)
target_include_directories( gle_ui PUBLIC src/ui src/ui/dialogs )
target_link_libraries( gle_ui PUBLIC Qt6::Widgets gle_core gle_scraper )

add_executable( GameListEditor
   src/main.cpp
   Resources/assets.qrc
)
target_link_libraries( GameListEditor PRIVATE gle_ui )

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
