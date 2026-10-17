#pragma once

class Gamelist;
class QWidget;

// What the window's views owe after a command.
//   None:    nothing changed.
//   Refresh: same games, new content: repaint.
//   Reset:   games added or removed, indices shifted: views start over.
enum class ViewUpdate
{
   None,
   Refresh,
   Reset,
};

// A command: asks the user, edits the gamelist, reports the view update owed.
// The caller guards unsaved edits first.
using SystemAction = ViewUpdate ( * )( QWidget* aParent, Gamelist& aGamelist );

ViewUpdate addMissingRoms( QWidget* aParent, Gamelist& aGamelist );
ViewUpdate linkExistingMedia( QWidget* aParent, Gamelist& aGamelist );
ViewUpdate renameMediaAfterRoms( QWidget* aParent, Gamelist& aGamelist );
ViewUpdate shrinkLargeImages( QWidget* aParent, Gamelist& aGamelist );
ViewUpdate deleteOrphans( QWidget* aParent, Gamelist& aGamelist );
ViewUpdate deleteDuplicates( QWidget* aParent, Gamelist& aGamelist );
