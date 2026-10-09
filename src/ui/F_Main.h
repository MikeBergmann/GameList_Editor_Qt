#pragma once

#include "Gamelist.h"

#include <QMainWindow>
#include <QPixmap>

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

// The main window: menus, the system combo, the game list and the settings.
//
// This is the orchestration half of F_Main.pas. It owns no game data - the
// Gamelist below is the single loaded system, the model addresses it by index,
// and the editing panels (steps 4.3 and 4.6) will take and return a GameFields
// value rather than being read field by field from here.
class Frm_Editor : public QMainWindow
{
   Q_OBJECT

public:
   explicit Frm_Editor( QWidget* aParent = nullptr );

   // FormShow's tips window (F_Main 651-659). Called once the window is up,
   // because it is modal.
   void showTipsAtStart();

   // Scan aRootPath and select its first system. The folder picker is the only
   // other caller; keeping the two apart is what lets the whole scan -> load ->
   // filter -> display chain be driven without a modal dialog in the way.
   void openRootFolder( const QString& aRootPath );

protected:
   void closeEvent( QCloseEvent* aEvent ) override;
   bool eventFilter( QObject* aWatched, QEvent* aEvent ) override;

private:
   void buildMenu();
   void buildCentralWidget();

   void LoadFromIni();
   void SaveToIni();

   void BuildSystemsList( bool aReload = false );
   void Cbx_SystemsChange();
   // Actions > Add missing ROMs.
   void addMissingRoms();
   void LoadSystemLogo( SystemKind aKind );
   void scaleSystemLogo();
   QString systemDisplayName( const SystemEntry& aSystem ) const;

   // Save / Discard / Cancel when the editor holds unsaved changes. False means
   // stay put: the user cancelled, or the save failed.
   bool confirmLeaveEdits();
   // Puts the list back on the games the editor is showing, after a Cancel.
   void restoreSelection( const QVector<int>& aIndices );

   // Cbx_FilterChange, Edt_SearchChange and both Chk_*Click were four handlers
   // that each re-ran the whole list rebuild. One spec, handed to the model.
   void refreshFilter();
   void updateCount();
   int currentGameIndex() const;
   QVector<int> selectedGameIndexes() const;

   // Btn_MoreInfosClick (3175). The panel says which game; the hash prompt is
   // here because FAutoHash is.
   void showMoreInfos( int aIndex );

   QString FRootPath;

   bool FGodMode = false;
   bool FAutoHash = false;
   bool FDelWoPrompt = false;
   bool FShowTips = false;
   bool FGenesisLogo = false;

   // Guards the filter/selection round trip: re-filtering moves the current row,
   // which would otherwise re-enter refreshFilter through the selection change.
   bool FIsLoading = false;

   Gamelist FGamelist;
   GamelistModel* FModel = nullptr;
   GamelistFilter* FProxy = nullptr;
   GameEditPanel* FEditPanel = nullptr;
   ScrapePanel* FScrapePanel = nullptr;
   QTabWidget* Pgc_Editor = nullptr;
   bool FRestoringSelection = false;

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
   QAction* Mnu_AddRoms = nullptr;
   QAction* Mnu_GodMode = nullptr;
   QAction* Mnu_DeleteWoPrompt = nullptr;
   QAction* Mnu_AutoHash = nullptr;
   QAction* Mnu_ShowTips = nullptr;
   QAction* Mnu_Genesis = nullptr;

   // Everything these three hold operates on the whole system or the selection,
   // so they stay disabled until step 4.4 gives them a Gamelist to write to.
   QMenu* Mnu_System = nullptr;
   QMenu* Mnu_Game = nullptr;
   QMenu* Mnu_Selection = nullptr;
};
