#pragma once

#include "Gamelist.h"
#include "SystemActions.h"

#include <QList>
#include <QMainWindow>
#include <QPixmap>

#include <functional>

class GameEditPanel;
class GamelistFilter;
class GamelistModel;
class ScrapePanel;
class QAction;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListView;
class QMenu;
class QTabWidget;

// The main window. Owns no game data: the Gamelist is the single loaded system.
class Frm_Editor : public QMainWindow
{
   Q_OBJECT

public:
   explicit Frm_Editor( QWidget* aParent = nullptr );

   // Modal, so call it once the window is up.
   void showTipsAtStart();

   // Scans aRootPath and selects its first system. Separate from the folder
   // picker so tests can drive it without a modal dialog.
   void openRootFolder( const QString& aRootPath );

protected:
   void closeEvent( QCloseEvent* aEvent ) override;
   bool eventFilter( QObject* aWatched, QEvent* aEvent ) override;

private:
   void buildMenu();
   void buildCentralWidget();
   QWidget* buildListColumn( QWidget* aParent );
   QTabWidget* buildEditorTabs( QWidget* aParent );
   void connectSignals();

   void onSelectionChanged();
   void onGameDeleted();
   void onGenesisToggled();

   void LoadFromIni();
   void SaveToIni();

   QAction* addOption( QMenu* aMenu, const QString& aText, const char* aKey, bool aDefault );

   void BuildSystemsList( bool aReload = false );
   void Cbx_SystemsChange();
   // The unguarded half: callers have already settled any unsaved edit.
   void loadSelectedSystem();
   SystemEntry systemAt( int aItem ) const;

   int findSystemItem( const std::function<bool( const SystemEntry& )>& aMatch ) const;
   // nullptr: nothing loaded.
   void bindGamelist( Gamelist* aGamelist );
   void runAction( SystemAction aAction );
   // After games were added or removed: indices shifted, so views start over.
   void resetViews();
   // After games changed content or media.
   void refreshViews();
   void setSystemActionsEnabled( bool aEnabled );
   void setSystemType();
   void setSelectionFlag( int GameFields::*aFlag, GameField aField, int aValue );
   void editSelectedNames();
   void LoadSystemLogo( SystemKind aKind );
   void scaleSystemLogo();
   void clearSystemLogo();

   // False means stay put: the user cancelled, or the save failed.
   bool confirmLeaveEdits();
   void restoreSelection( const QVector<int>& aIndices );

   void refreshFilter();
   void updateCount();
   int currentGameIndex() const;
   QVector<int> selectedGameIndexes() const;

   void showMoreInfos( int aIndex );

   QString FRootPath;

   // Re-filtering moves the current row, which would re-enter refreshFilter.
   bool FApplyingFilter = false;

   Gamelist FGamelist;
   GamelistModel* FModel = nullptr;
   GamelistFilter* FProxy = nullptr;
   GameEditPanel* FEditPanel = nullptr;
   ScrapePanel* FScrapePanel = nullptr;
   QTabWidget* Pgc_Editor = nullptr;
   bool FRestoringSelection = false;
   bool FScrapeBusy = false;

   QComboBox* Cbx_Systems = nullptr;
   QComboBox* Cbx_Filter = nullptr;
   QLineEdit* Edt_Search = nullptr;
   QCheckBox* Chk_ListByRom = nullptr;
   QCheckBox* Chk_FullRomName = nullptr;
   QListView* Lbx_Games = nullptr;
   QLabel* Img_System = nullptr;
   QPixmap FSystemLogo;
   QLabel* Lbl_NbGamesFound = nullptr;

   QAction* Mnu_Reload = nullptr;
   QList<QAction*> FSystemActions;  // off while no system is loaded
   QList<QAction*> FSelectionActions;  // off while no game is selected
   QAction* Mnu_GodMode = nullptr;
   QAction* Mnu_DeleteWoPrompt = nullptr;
   QAction* Mnu_AutoHash = nullptr;
   QAction* Mnu_ShowTips = nullptr;
   QAction* Mnu_Genesis = nullptr;
   QAction* Mnu_AutoplayVideo = nullptr;

   struct Option
   {
      QAction* action;
      const char* key;
      bool fallback;
   };
   QList<Option> FOptions;
};
