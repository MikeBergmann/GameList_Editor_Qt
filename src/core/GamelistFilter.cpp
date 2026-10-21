#include "GamelistFilter.h"

#include "Gamelist.h"

#include <QDate>
#include <QFileInfo>

namespace {

// In FilterCategory order.
const char* const Cst_FilterNames[] = {
   "All",
   "Missing Picture",
   "Missing Video",
   "Missing Date",
   "Missing Number of Players",
   "Missing Rating",
   "Missing Developer",
   "Missing Publisher",
   "Missing Description",
   "Missing Genre",
   "Missing Region",
   "Kid Game",
   "Hidden",
   "Favorite",
   "Orphan",
   "Same Region",
   "Same Date",
   "Same or Earlier Date",
   "Same or Later Date",
   "Same Players",
   "Same Rating",
   "Same or Lower Rating",
   "Same or Higher Rating",
   "Same Publisher",
   "Same Developer",
   "Same Genre",
   "Same Folder",
   "Same Name",
   "Same ROM",
   "Duplicate Names",
   "Duplicate ROMs",
};

static_assert( sizeof( Cst_FilterNames ) / sizeof( Cst_FilterNames[0] ) ==
                  static_cast<size_t>( FilterCategory::Count ),
               "one name per FilterCategory, in enum order" );

// Ratings are written with either decimal separator; an unparseable one counts as 0.
double ratingValue( const QString& aRating )
{
   QString text = aRating;
   return text.replace( QLatin1Char( ',' ), QLatin1Char( '.' ) ).toDouble();
}

QDate releaseDateValue( const QString& aDisplayDate )
{
   switch ( aDisplayDate.size() ) {
      case 4: return QDate::fromString( aDisplayDate, QStringLiteral( "yyyy" ) );
      case 7: return QDate::fromString( aDisplayDate, QStringLiteral( "MM/yyyy" ) );
      case 10: return QDate::fromString( aDisplayDate, QStringLiteral( "dd/MM/yyyy" ) );
      default: return {};
   }
}

bool matchesCategory( const Gamelist& aList, const Game& aGame, const FilterSpec& aFilter )
{
   // The "same as" categories compare against the selected game; with none selected
   // nothing matches.
   const bool needsReference = filterDependsOnSelection( aFilter.category );
   if ( needsReference && ( aFilter.reference < 0 || aFilter.reference >= aList.count() ) )
      return false;

   // Only read by the categories above, for which it is valid. No default in the
   // switch below, so the compiler flags a category added to the enum and not handled.
   const Game& reference = needsReference ? aList.at( aFilter.reference ) : aGame;

   switch ( aFilter.category ) {
      case FilterCategory::All: return true;
      case FilterCategory::MissingImage: return aGame.missingImage;
      case FilterCategory::MissingVideo: return aGame.missingVideo;
      case FilterCategory::MissingDate: return aGame.releaseDate.isEmpty();
      case FilterCategory::MissingPlayers: return aGame.players.isEmpty();
      case FilterCategory::MissingRating: return aGame.rating.isEmpty();
      case FilterCategory::MissingDeveloper: return aGame.developer.isEmpty();
      case FilterCategory::MissingPublisher: return aGame.publisher.isEmpty();
      case FilterCategory::MissingDescription: return aGame.description.isEmpty();
      case FilterCategory::MissingGenre: return aGame.genre.isEmpty();
      case FilterCategory::MissingRegion: return aGame.region.isEmpty();
      case FilterCategory::KidGame: return aGame.kidGame == 1;
      case FilterCategory::Hidden: return aGame.hidden == 1;
      case FilterCategory::Favorite: return aGame.favorite == 1;
      case FilterCategory::Orphan: return aGame.isOrphan;
      case FilterCategory::DuplicateNames: return aList.nameIsDuplicated( aGame.name );
      case FilterCategory::DuplicateRoms: return aList.romIsDuplicated( aGame.romName() );

      case FilterCategory::SameRegion: return aGame.region == reference.region;
      case FilterCategory::SameDate: return aGame.releaseDate == reference.releaseDate;
      case FilterCategory::SameOrEarlierDate:
      case FilterCategory::SameOrLaterDate: {
         if ( aGame.releaseDate.isEmpty() )
            return false;
         // An unset reference date counts as today.
         QDate referenceDate = releaseDateValue( reference.releaseDate );
         if ( !referenceDate.isValid() )
            referenceDate = QDate::currentDate();
         const QDate date = releaseDateValue( aGame.releaseDate );
         return aFilter.category == FilterCategory::SameOrEarlierDate ? date <= referenceDate
                                                                      : date >= referenceDate;
      }
      case FilterCategory::SamePlayers: return aGame.players == reference.players;
      case FilterCategory::SameRating: return aGame.rating == reference.rating;
      case FilterCategory::SameOrLowerRating:
         return !aGame.rating.isEmpty() &&
                ratingValue( aGame.rating ) <= ratingValue( reference.rating );
      case FilterCategory::SameOrHigherRating:
         return !aGame.rating.isEmpty() &&
                ratingValue( aGame.rating ) >= ratingValue( reference.rating );
      case FilterCategory::SamePublisher: return aGame.publisher == reference.publisher;
      case FilterCategory::SameDeveloper: return aGame.developer == reference.developer;
      case FilterCategory::SameGenre: return aGame.genre == reference.genre;
      // Compared as written, not case-insensitively: on Linux two distinct directories
      // can differ only in case. The case is already settled against the disk by
      // resolvePath.
      case FilterCategory::SameFolder:
         return QFileInfo( aGame.physicalRomPath ).path() ==
                QFileInfo( reference.physicalRomPath ).path();
      case FilterCategory::SameName: return aGame.name == reference.name;
      case FilterCategory::SameRom: return aGame.romName() == reference.romName();

      case FilterCategory::Count: break;
   }

   return false;
}

}  // namespace

QString displayName( const Gamelist& aList, int aIndex, const FilterSpec& aFilter )
{
   const Game& game = aList.at( aIndex );

   if ( !aFilter.listByRom )
      return game.name;
   if ( !aFilter.fullRomName )
      return game.romName();

   QString path = game.romPath;
   if ( path.startsWith( QLatin1String( "./" ) ) )
      path.remove( 0, 2 );
   return path;
}

bool filterAccepts( const Gamelist& aList, int aIndex, const FilterSpec& aFilter )
{
   if ( aIndex < 0 || aIndex >= aList.count() )
      return false;

   if ( !matchesCategory( aList, aList.at( aIndex ), aFilter ) )
      return false;

   // Case-insensitive, against whatever the list displays.
   return aFilter.search.isEmpty() ||
          displayName( aList, aIndex, aFilter ).contains( aFilter.search, Qt::CaseInsensitive );
}

const char* filterCategoryName( FilterCategory aCategory )
{
   return Cst_FilterNames[static_cast<size_t>( aCategory )];
}
