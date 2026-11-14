# Core, scraper and the controllers; the Widgets front end and the tests do not build here.
# Included from the top-level CMakeLists.txt, which then returns.

qt_policy( SET QTP0001 NEW )
qt_policy( SET QTP0002 NEW )

# versionCode must grow with every release or Android refuses the update. Only a "vX.Y.Z" tag
# gets a real one (X*10000 + Y*100 + Z); anything else is 1, so a dev build never outranks a release.
set( GLE_ANDROID_VERSION_CODE 1 )
if( GLE_VERSION MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)" )
   math( EXPR GLE_ANDROID_VERSION_CODE "${CMAKE_MATCH_1} * 10000 + ${CMAKE_MATCH_2} * 100 + ${CMAKE_MATCH_3}" )
endif()
string( REGEX REPLACE "^v" "" GLE_ANDROID_VERSION_NAME "${GLE_VERSION}" )

# Release signing: androiddeployqt reads QT_ANDROID_KEYSTORE_PATH / _ALIAS / _STORE_PASS /
# _KEY_PASS from the environment, so the keystore and its passwords never touch the repo.
if( DEFINED ENV{QT_ANDROID_KEYSTORE_PATH} )
   set( QT_ANDROID_SIGN_APK ON )
endif()
qt_add_executable( GameListEditor src/mobile/main_android.cpp src/mobile/AndroidStorage.cpp
   src/mobile/ScrapeImageProvider.cpp Resources/assets.qrc )
target_compile_definitions( GameListEditor PRIVATE GLE_VERSION="${GLE_VERSION}" )
target_link_libraries( GameListEditor PRIVATE gle_mobile_controllers gle_scraper Qt6::Quick Qt6::QuickControls2 Qt6::Multimedia )
qt_add_qml_module( GameListEditor
   URI GleMobile
   VERSION 1.0
   QML_FILES
      src/mobile/qml/AboutPage.qml
      src/mobile/qml/ActionsPage.qml
      src/mobile/qml/AddRomsPage.qml
      src/mobile/qml/AskDialog.qml
      src/mobile/qml/BarButton.qml
      src/mobile/qml/EditPage.qml
      src/mobile/qml/FieldDialog.qml
      src/mobile/qml/FileBrowserPage.qml
      src/mobile/qml/GamesPage.qml
      src/mobile/qml/HelpPage.qml
      src/mobile/qml/InfoDialog.qml
      src/mobile/qml/KindDialog.qml
      src/mobile/qml/Main.qml
      src/mobile/qml/PermissionPage.qml
      src/mobile/qml/ScrapePage.qml
      src/mobile/qml/SettingsPage.qml
      src/mobile/qml/SystemsPage.qml
      src/mobile/qml/TextPage.qml
      src/mobile/qml/VideoDialog.qml
)

# Qt's TLS backend on Android is OpenSSL, and the device does not provide a usable one.
include( "${ANDROID_SDK_ROOT}/android_openssl/android_openssl.cmake" )
add_android_openssl_libraries( GameListEditor )

set_target_properties( GameListEditor PROPERTIES
   QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
   QT_ANDROID_PACKAGE_NAME "de.mdb977.gamelisteditor"
   QT_ANDROID_MIN_SDK_VERSION 33
   QT_ANDROID_VERSION_CODE ${GLE_ANDROID_VERSION_CODE}
   QT_ANDROID_VERSION_NAME "${GLE_ANDROID_VERSION_NAME}"
)
