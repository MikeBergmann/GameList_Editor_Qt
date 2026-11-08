# Android: only core and scraper build; the Widgets front end and the tests do not.
# Included from the top-level CMakeLists.txt, which then returns.

qt_policy( SET QTP0002 NEW )
qt_add_executable( gle_smoke src/mobile/main_android.cpp )
target_link_libraries( gle_smoke PRIVATE gle_scraper )
set_target_properties( gle_smoke PROPERTIES
   QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
   QT_ANDROID_PACKAGE_NAME "de.mdb977.gamelisteditor"
   QT_ANDROID_MIN_SDK_VERSION 33
)
