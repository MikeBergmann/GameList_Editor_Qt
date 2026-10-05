#include "NameEdit.h"

QString applyNameEdit( const QString& aName, const NameEdit& aEdit )
{
   QString result = aName;

   if ( aEdit.removeChars ) {
      // Both bounds are clamped. The Delphi's Copy/SetLength pair was unguarded,
      // so asking to remove more characters than the name has produced a
      // SetLength with a negative length rather than an empty name - reachable
      // from the UI by typing a large number into either field.
      result = result.mid( qBound( 0, aEdit.nbStart, result.size() ) );
      result.chop( qBound( 0, aEdit.nbEnd, result.size() ) );
   }

   // Guarding on empty matters: the Delphi wrote TmpStr[1] unconditionally,
   // which on an empty string is a write past the end.
   if ( aEdit.changeCase && !result.isEmpty() ) {
      switch ( aEdit.caseIndex ) {
         // Delphi's UpCase was ASCII-only; QChar::toUpper is not, so an
         // accented first letter now capitalises instead of being left alone.
         case 0: result[0] = result[0].toUpper(); break;
         case 1: result = result.toUpper(); break;
         case 2: result = result.toLower(); break;
         default: break;  // -1: the group is on but no option was picked.
      }
   }

   if ( aEdit.addChars )
      result = aEdit.startString + result + aEdit.endString;

   return result;
}
