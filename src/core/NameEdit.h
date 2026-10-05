#pragma once

#include <QString>

// What the batch rename dialog collected: the Delphi returned this as eight
// out-parameters.
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

// The transformation itself. It lived twice in the Delphi - once as
// F_AdvNameEditor.ProcessPreview for the live preview, once as
// F_Main.TransformGamesNames to apply it - so the preview could disagree with
// the result. One implementation, used by both.
QString applyNameEdit( const QString& aName, const NameEdit& aEdit );
