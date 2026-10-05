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
find_package( Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Xml )
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

add_library( gle_ui STATIC
   src/ui/GameEditPanel.cpp
   src/ui/F_Main.cpp
   src/ui/dialogs/F_About.cpp
   src/ui/dialogs/F_AdvNameEditor.cpp
   src/ui/dialogs/F_ConfigureNetwork.cpp
   src/ui/dialogs/F_Help.cpp
   src/ui/dialogs/F_MoreInfos.cpp
)
target_include_directories( gle_ui PUBLIC src/ui src/ui/dialogs )
target_link_libraries( gle_ui PUBLIC Qt6::Widgets gle_core )

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
   Resources/assets.qrc
)
target_include_directories( tests PRIVATE tests )
target_link_libraries( tests PRIVATE gle_core gle_ui GTest::gtest )

enable_testing()
gtest_discover_tests( tests )
