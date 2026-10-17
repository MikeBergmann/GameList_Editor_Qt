#pragma once

#include <QString>

#include <optional>
#include <utility>

// What a fallible operation returns instead of "bool plus QString* out-parameter":
// either a value (Result<void> has none) or an Error with a message fit for a dialog.
// Nodiscard, so an ignored failure is a warning, not a silent one.
//
//    Result<int> addGames( ... );
//    ...
//    const Result<int> added = list.addGames( paths );
//    if ( !added )
//       warn( added.error() );
//    else
//       use( added.value() );
//
// An Error is built with `return Error{ message };`, success with `return value;`
// (or `return {};` for Result<void>).
//
// A failed Result of any type converts to a failed Result of any other, so a caller
// passes a failure up with `return saved;`, not `return Error{ saved.error() };`.
// A Result<void> target also accepts a success (the value is dropped); any other
// target does not, because there would be no value to give it.
struct Error
{
   QString message;
};

template <typename T = void>
class [[nodiscard]] Result
{
public:
   Result( T aValue ) : FValue( std::move( aValue ) ) {}
   Result( Error aError ) : FError( std::move( aError.message ) ) {}

   template <typename U>
   Result( const Result<U>& aFailed ) : FError( aFailed.error() )
   {
      Q_ASSERT_X( !aFailed, "Result", "only a failed Result converts to a Result with a value" );
   }

   explicit operator bool() const { return FValue.has_value(); }

   // Only valid on success.
   const T& value() const { return *FValue; }
   // Empty on success.
   const QString& error() const { return FError; }

private:
   std::optional<T> FValue;
   QString FError;
};

template <>
class [[nodiscard]] Result<void>
{
public:
   Result() = default;
   Result( Error aError ) : FError( std::move( aError.message ) ), FFailed( true ) {}

   template <typename U>
   Result( const Result<U>& aOther ) : FError( aOther.error() ), FFailed( !aOther )
   {
   }

   explicit operator bool() const { return !FFailed; }

   const QString& error() const { return FError; }

private:
   QString FError;
   bool FFailed = false;
};
