#pragma once

#include "Gamelist.h"

#include <QDialog>
#include <QVector>

class QListWidget;

// Reviews the pictures and videos Gamelist::unlinkedMedia found: one ticked row
// per file, so a wrong guess can be unticked before anything is written.
class Frm_LinkMedia : public QDialog
{
   Q_OBJECT

public:
   explicit Frm_LinkMedia( QWidget* aParent = nullptr );

   // The ticked links. Empty if the user cancelled.
   QVector<MediaLink> Execute( const Gamelist& aGamelist, const QVector<MediaLink>& aCandidates );

private:
   QListWidget* Lst_Media = nullptr;
};
