#pragma once

#include <QString>

// What the filter combo offers. The enumerators are in the order the combo lists them.
// SameRegion..SameRom are "same as the selected game", and must stay contiguous:
// filterDependsOnSelection() is a range check.
enum class FilterCategory
{
   All,
   MissingImage,
   MissingVideo,
   MissingDate,
   MissingPlayers,
   MissingRating,
   MissingDeveloper,
   MissingPublisher,
   MissingDescription,
   MissingGenre,
   MissingRegion,
   KidGame,
   Hidden,
   Favorite,
   Orphan,
   SameRegion,
   SameDate,
   SameOrEarlierDate,
   SameOrLaterDate,
   SamePlayers,
   SameRating,
   SameOrLowerRating,
   SameOrHigherRating,
   SamePublisher,
   SameDeveloper,
   SameGenre,
   SameFolder,
   SameName,
   SameRom,
   DuplicateNames,
   DuplicateRoms,
   Count,  // not a category: the number of them
};

const char* filterCategoryName( FilterCategory aCategory );

// Whether the answer depends on which game is selected, so a view must re-run the
// filter when the selection changes.
inline bool filterDependsOnSelection( FilterCategory aCategory )
{
   return aCategory >= FilterCategory::SameRegion && aCategory <= FilterCategory::SameRom;
}

struct FilterSpec
{
   FilterCategory category = FilterCategory::All;
   QString search;
   bool listByRom = false;
   bool fullRomName = false;

   // Index of the selected game. The "same as" categories compare against it, so a
   // view must re-run the filter when the selection changes.
   int reference = -1;
};

class Gamelist;

// What the list shows for that game, and what the search text matches.
QString displayName( const Gamelist& aList, int aIndex, const FilterSpec& aFilter );

// The filter predicate, category and search text together. False for a bad index.
bool filterAccepts( const Gamelist& aList, int aIndex, const FilterSpec& aFilter );
