#pragma once

#include <QDialog>

class QTextBrowser;

// The "About..." box.
class Frm_About : public QDialog
{
   Q_OBJECT

public:
   explicit Frm_About( QWidget* aParent = nullptr );

private:
   QTextBrowser* Red_About = nullptr;
};
