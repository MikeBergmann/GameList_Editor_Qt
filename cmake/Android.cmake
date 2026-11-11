# Core, scraper and the controllers; the Widgets front end and the tests do not build here.
# Included from the top-level CMakeLists.txt, which then returns.

qt_policy( SET QTP0001 NEW )
qt_policy( SET QTP0002 NEW )
qt_add_executable( gle_smoke src/mobile/main_android.cpp src/mobile/AndroidStorage.cpp
   src/mobile/ScrapeImageProvider.cpp Resources/assets.qrc )
target_link_libraries( gle_smoke PRIVATE gle_mobile_controllers gle_scraper Qt6::Quick Qt6::QuickControls2 )
qt_add_qml_module( gle_smoke
   URI GleMobile
   VERSION 1.0
   QML_FILES
      src/mobile/qml/AskDialog.qml
      src/mobile/qml/EditPage.qml
      src/mobile/qml/FieldDialog.qml
      src/mobile/qml/FileBrowserPage.qml
      src/mobile/qml/GamesPage.qml
      src/mobile/qml/Main.qml
      src/mobile/qml/PermissionPage.qml
      src/mobile/qml/ScrapePage.qml
      src/mobile/qml/SettingsPage.qml
      src/mobile/qml/SystemsPage.qml
)

# Qt's TLS backend on Android is OpenSSL, and the device does not provide a usable one.
include( "${ANDROID_SDK_ROOT}/android_openssl/android_openssl.cmake" )
add_android_openssl_libraries( gle_smoke )

set_target_properties( gle_smoke PROPERTIES
   QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
   QT_ANDROID_PACKAGE_NAME "de.mdb977.gamelisteditor"
   QT_ANDROID_MIN_SDK_VERSION 33
)
