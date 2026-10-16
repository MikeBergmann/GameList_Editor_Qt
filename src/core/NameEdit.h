#pragma once

#include <QString>

// What the batch rename dialog collected.
struct NameEdit
{
   bool removeChars = false;
   int nbStart = 0;
   int nbEnd = 0;

   bool changeCase = false;
   int caseIndex = -1;  // 0 capitalise first, 1 uppercase, 2 lowercase; -1 none

   bool addChars = false;
   QString startString;
   QString endString;
};

// Applies the edit to one name: remove characters, change case, then add text.
// Shared by the dialog's preview and the real rename, so they cannot disagree.
QString applyNameEdit( const QString& aName, const NameEdit& aEdit );
