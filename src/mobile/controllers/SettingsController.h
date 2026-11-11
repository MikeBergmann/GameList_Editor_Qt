#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>

// The ScreenScraper login, scrape language and proxy: the keys Frm_Network edits on the desktop.
// Every change is written to QSettings at once, so there is nothing to save or discard.
class SettingsController : public QObject
{
   Q_OBJECT
   Q_PROPERTY( QVariantMap values READ values NOTIFY valuesChanged )
   Q_PROPERTY( QStringList languageNames READ languageNames CONSTANT )

public:
   explicit SettingsController( QObject* aParent = nullptr );

   // ssUser, ssPwd, language (index into languageNames), proxyUse, proxyServer, proxyPort,
   // proxyUser, proxyPwd.
   QVariantMap values() const;
   QStringList languageNames() const;

   // Unknown keys are ignored.
   Q_INVOKABLE void setValue( const QString& aKey, const QVariant& aValue );

signals:
   void valuesChanged();
};
