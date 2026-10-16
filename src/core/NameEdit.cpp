#include "NameEdit.h"

QString applyNameEdit( const QString& aName, const NameEdit& aEdit )
{
   QString result = aName;

   if ( aEdit.removeChars ) {
      // Clamped, so removing more characters than the name has gives an empty name.
      result = result.mid( qBound( 0, aEdit.nbStart, result.size() ) );
      result.chop( qBound( 0, aEdit.nbEnd, result.size() ) );
   }

   // Not on an empty string: there is no first character to write to.
   if ( aEdit.changeCase && !result.isEmpty() ) {
      switch ( aEdit.caseIndex ) {
         // QChar::toUpper is not ASCII-only, so an accented first letter capitalises too.
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
