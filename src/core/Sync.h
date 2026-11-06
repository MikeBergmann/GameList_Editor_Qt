#pragma once

#include "Game.h"
#include "RomCopy.h"
#include "Systems.h"

#include <QString>
#include <QStringList>
#include <QVector>

// One-way sync of a master collection into another folder, whichever layout either side
// uses. This unit only reads: planSync says what a run would do.

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
   bool copyImage = false;
   bool copyVideo = false;
   QString imageSource;   // absolute, when copyImage
   QString videoSource;   // absolute, when copyVideo
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

   bool isEmpty() const { return games.isEmpty(); }
};

struct SyncPlan
{
   QVector<SystemPlan> systems;  // those with something to do or to report

   bool isEmpty() const;
};

SystemPlan planSystem( const SystemPair& aPair );

// Loads and compares every pair; nothing is written.
SyncPlan planSync( const QVector<SystemEntry>& aMaster, const QString& aTargetRoot );
