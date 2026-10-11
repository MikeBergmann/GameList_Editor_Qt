#pragma once

#include "Gamelist.h"

#include <QDialog>
#include <QVector>

class QCheckBox;
class QLabel;
class QListWidget;

// Reviews the pictures and videos Gamelist::unlinkedMedia found: one ticked row
// per file, so a wrong guess can be unticked before anything is written. The same
// list reviews the renames Gamelist::misnamedMedia proposes.
class Frm_LinkMedia : public QDialog
{
   Q_OBJECT

public:
   explicit Frm_LinkMedia( QWidget* aParent = nullptr );

   // The ticked links. Empty if the user cancelled.
   QVector<MediaLink> Execute( const Gamelist& aGamelist, const QVector<MediaLink>& aCandidates );

   // The ticked renames. Rows with a problem are shown, unticked and disabled.
   // Lists the games' misnamed media itself, again when the strict box is toggled.
   QVector<MediaRename> ExecuteRename( const Gamelist& aGamelist );

   // The ticked pictures. The files are overwritten, so the intro says so.
   QVector<OversizedImage> ExecuteShrink( const Gamelist& aGamelist, qint64 aMaxBytes,
                                          const QVector<OversizedImage>& aCandidates );

private:
   struct Row
   {
      QString text;
      bool enabled = true;
   };

   void fill( const QVector<Row>& aRows );

   // The ticked row numbers; empty if the user cancelled.
   QVector<int> review( const QString& aTitle, const QString& aIntro, const QVector<Row>& aRows );

   QLabel* Lbl_Intro = nullptr;
   QCheckBox* Chk_Strict = nullptr;
   QListWidget* Lst_Media = nullptr;
};
