#pragma once

#include "Systems.h"

#include <optional>

#include <QStringList>

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

// Asks which system the folder is, saves the mapping and returns the kind (skOther forgets
// it). Empty when the user cancels or the folder name is already a known system.
std::optional<SystemKind> chooseSystemKind( QWidget* aParent, const SystemEntry& aSystem );

// Touches no game, so it never owes the views anything and needs no save-first guard.
void cleanUpBackups( QWidget* aParent, const Gamelist& aGamelist );

// Writes aNames, one per line, to a file the user picks. Touches no game, like cleanUpBackups.
void exportGamesList( QWidget* aParent, const Gamelist& aGamelist, const QStringList& aNames );
