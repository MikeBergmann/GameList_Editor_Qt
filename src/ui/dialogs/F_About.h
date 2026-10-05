#pragma once

#include <QDialog>

class QTextBrowser;

// The "About..." box. The Delphi subclassed TRichEdit and hand-rolled URL
// detection out of EM_AUTOURLDETECT, EN_LINK and ShellExecute; QTextBrowser
// opens links on its own, so all of that is gone.
class Frm_About : public QDialog
{
   Q_OBJECT

public:
   explicit Frm_About( QWidget* aParent = nullptr );

private:
   QTextBrowser* Red_About = nullptr;
};
