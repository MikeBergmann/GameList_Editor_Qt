#pragma once

class Gamelist;
class QWidget;

// What an Actions > System command did, as far as the window's views are concerned.
//   None:    nothing changed, or the user cancelled.
//   Refresh: same games, different content or media: repaint the list and reload the editor.
//   Reset:   games were added or removed, so indices shifted: the views start over.
enum class ViewUpdate
{
   None,
   Refresh,
   Reset,
};

// A command: asks the user, edits the gamelist (each step saves by itself) and says which
// view update the window owes. The window guards unsaved edits before calling one.
using SystemAction = ViewUpdate ( * )( QWidget* aParent, Gamelist& aGamelist );

ViewUpdate addMissingRoms( QWidget* aParent, Gamelist& aGamelist );
ViewUpdate linkExistingMedia( QWidget* aParent, Gamelist& aGamelist );
ViewUpdate renameMediaAfterRoms( QWidget* aParent, Gamelist& aGamelist );
ViewUpdate shrinkLargeImages( QWidget* aParent, Gamelist& aGamelist );
ViewUpdate deleteOrphans( QWidget* aParent, Gamelist& aGamelist );
ViewUpdate deleteDuplicates( QWidget* aParent, Gamelist& aGamelist );
