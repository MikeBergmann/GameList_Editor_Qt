#pragma once

#include <QObject>

// All-files access: the one permission the app cannot work without (Android.md, Phase 2).
class AndroidStorage : public QObject
{
   Q_OBJECT
   Q_PROPERTY( bool hasAccess READ hasAccess NOTIFY hasAccessChanged )

public:
   explicit AndroidStorage( QObject* aParent = nullptr );

   bool hasAccess() const { return FHasAccess; }

   // Opens the system page for this app; hasAccess follows when the user comes back.
   Q_INVOKABLE void request();

signals:
   void hasAccessChanged();

private:
   void refresh();

   bool FHasAccess = false;
};
