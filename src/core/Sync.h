#pragma once

#include "Game.h"
#include "RomCopy.h"
#include "Systems.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

// One-way sync of a master collection into another folder, whichever layout either side
// uses. planSync only reads and says what a run would do; applySync does it.

// The fields a sync copies. Favorite, hidden, kidgame, playcount, lastplayed and every child
// the editor does not know stay per device.
GameFieldSet syncedFields();

struct SystemPair
{
   SystemEntry source;
   SystemEntry target;
   bool targetExists = false;  // false: the target has no gamelist.xml yet
};

// Each master system with its partner under aTargetRoot: the same folder name, else the same
// SystemKind (never skOther). One without a partner gets the entry the target would have.
// megadrive and genesis are two kinds and are not paired.
QVector<SystemPair> pairSystems( const QVector<SystemEntry>& aMaster, const QString& aTargetRoot );

// A game that needs something done on the target.
struct SyncGame
{
   int sourceIndex = -1;
   int targetIndex = -1;  // -1: new on the target
   QString name;
   QString romLink;       // the master's <path>
   RomCopyPlan rom;       // empty when the ROM folders are the same one
   GameFieldSet fields;   // what to write: what differs, or what is set for a new game
   QString imageSource;   // absolute; empty: no picture to copy
   QString videoSource;   // absolute; empty: no video to copy
};

struct SystemPlan
{
   SystemPair pair;
   bool createTarget = false;  // the gamelist (and folders) must be made first
   bool copyRoms = false;      // the target's ROM folder is another folder
   QVector<SyncGame> games;    // only those with something to do
   int unchanged = 0;
   QStringList targetOnly;     // names of target games the master does not have; never removed
   QStringList problems;       // what was left out and why; not an action
   qint64 romBytes = 0;
   bool canceled = false;      // the user stopped the analysis; the plan is partial

   bool isEmpty() const { return games.isEmpty(); }
};

struct SyncPlan
{
   QVector<SystemPlan> systems;  // those with something to do or to report
   bool canceled = false;        // the user stopped the analysis; systems is empty

   bool isEmpty() const;
};

// Called while analysing or applying, with what is being done; returning false cancels.
using SyncProgress = std::function<bool( const QString& aMessage )>;

// aDone of aTotal systems are finished; the message names the one being read.
using PlanProgress = std::function<bool( const QString& aMessage, int aDone, int aTotal )>;

SystemPlan planSystem( const SystemPair& aPair, const SyncProgress& aProgress = {} );

// Loads and compares every pair; nothing is written. Progress runs before each game, so a
// slow media hash shows which game it is on; a canceled plan is empty.
SyncPlan planSync( const QVector<SystemEntry>& aMaster, const QString& aTargetRoot,
                   const PlanProgress& aProgress = {} );

// What one system's run did. A non-empty error means it stopped there; what was already
// copied stays, and a second sync picks up the rest.
struct SystemReport
{
   QString name;
   int added = 0;        // games put on the target's gamelist
   int updated = 0;      // existing games that got fields, a ROM or media
   int mediaCopied = 0;
   QStringList skipped;  // one line per game or file left out, with the reason
   QString error;
   bool canceled = false;

   bool ok() const { return error.isEmpty(); }
};

struct SyncReport
{
   QVector<SystemReport> systems;
   bool canceled = false;
};

// Per system: free space, ROM files, the gamelist (made first if needed), fields, media.
// Progress runs before each ROM file, each media file and each write stage.
// A failing system is reported and the next one still runs. Games are found by the indices
// the plan holds; a master or target changed since then is reported, not guessed at.
// ponytail: cancel and progress are per file, not per byte; a big ROM runs to its end.
SystemReport applySystem( const SystemPlan& aPlan, const SyncProgress& aProgress = {} );
SyncReport applySync( const SyncPlan& aPlan, const SyncProgress& aProgress = {} );

// How many times applySync will call its progress callback for aPlan, for a progress bar's range.
int syncStepCount( const SyncPlan& aPlan );
