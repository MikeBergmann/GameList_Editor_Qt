#include "SettingsController.h"

#include "Resources.h"

#include <QSettings>

namespace {

// Order is LangName's, which makes the combo index the stored value.
constexpr const char* Cst_LangNameFull[lnCount] = { "French", "German", "English", "Spanish",
                                                    "Portuguese (BR)" };

constexpr int Cst_MaxPort = 65535;

struct Key
{
   const char* name;
   QLatin1StringView ini;
};

const Key Cst_Keys[] = {
   { "ssUser", Cst_IniSSUser },           { "ssPwd", Cst_IniSSPwd },
   { "language", Cst_IniLanguage },       { "proxyUse", Cst_IniProxyUse },
   { "proxyServer", Cst_IniProxyServer }, { "proxyPort", Cst_IniProxyPort },
   { "proxyUser", Cst_IniProxyUser },     { "proxyPwd", Cst_IniProxyPwd },
};

}  // namespace

SettingsController::SettingsController( QObject* aParent ) : QObject( aParent ) {}

QStringList SettingsController::languageNames() const
{
   QStringList names;
   for ( const char* name : Cst_LangNameFull )
      names << QLatin1String( name );
   return names;
}

QVariantMap SettingsController::values() const
{
   QSettings settings;
   settings.beginGroup( Cst_IniOptions );

   QVariantMap result;
   for ( const Key& key : Cst_Keys )
      result.insert( QLatin1String( key.name ), settings.value( key.ini ).toString() );

   result[QStringLiteral( "language" )] = static_cast<int>(
      langFromIndex( settings.value( Cst_IniLanguage, Cst_IniLanguageDefault ).toInt() ) );
   result[QStringLiteral( "proxyUse" )] = settings.value( Cst_IniProxyUse, false ).toBool();
   // 0 is stored for "no port given"; show it as an empty field.
   const int port = settings.value( Cst_IniProxyPort, 0 ).toInt();
   result[QStringLiteral( "proxyPort" )] = port > 0 ? QString::number( port ) : QString();
   return result;
}

void SettingsController::setValue( const QString& aKey, const QVariant& aValue )
{
   for ( const Key& key : Cst_Keys ) {
      if ( aKey != QLatin1String( key.name ) )
         continue;

      QVariant value = aValue;
      if ( key.ini == Cst_IniLanguage )
         value = static_cast<int>( langFromIndex( aValue.toInt() ) );
      else if ( key.ini == Cst_IniProxyUse )
         value = aValue.toBool();
      else if ( key.ini == Cst_IniProxyPort )
         // "0" means no port given, and an empty value would read back as an empty string.
         value = qBound( 0, aValue.toString().toInt(), Cst_MaxPort );

      QSettings settings;
      settings.beginGroup( Cst_IniOptions );
      settings.setValue( key.ini, value );
      emit valuesChanged();
      return;
   }
}
