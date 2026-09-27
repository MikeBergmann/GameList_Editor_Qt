cmake_minimum_required( VERSION 3.21 )

project( GameListEditor LANGUAGES CXX )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_AUTOMOC ON )
set( CMAKE_AUTORCC ON )

# For clangd: it looks for compile_commands.json next to the sources, so the
# generated one is symlinked into the source tree below.
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

# 6.4 is what Ubuntu 24.04 / Mint 22 ships; nothing here needs more.
find_package( Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Xml )
find_package( ZLIB REQUIRED )
find_package( GTest REQUIRED )

include( GoogleTest )

add_library( gle_core STATIC
   src/core/Game.cpp
   src/core/Gamelist.cpp
   src/core/Resources.cpp
)
target_include_directories( gle_core PUBLIC src/core )
target_link_libraries( gle_core PUBLIC Qt6::Core Qt6::Gui Qt6::Xml ZLIB::ZLIB )

add_executable( tests
   tests/main.cpp
   tests/tst_game.cpp
   tests/tst_gamelist.cpp
   tests/tst_resources.cpp
   Resources/assets.qrc
)
target_include_directories( tests PRIVATE tests )
target_link_libraries( tests PRIVATE gle_core Qt6::Widgets GTest::gtest )

enable_testing()
gtest_discover_tests( tests )
