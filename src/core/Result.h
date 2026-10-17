#pragma once

#include <QString>

#include <optional>
#include <utility>

// A fallible operation's result: a value (none for Result<void>) or an Error
// with a message fit for a dialog.
//
//    const Result<int> added = list.addGames( paths );
//    if ( !added )
//       warn( added.error() );
//
// Fail with `return Error{ message };`. A failed Result converts to any other
// Result type, so pass one up with `return saved;`.
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
      // Release skips the assert: still a failure, with a reason.
      if ( aFailed && FError.isEmpty() )
         FError = QStringLiteral(
            "Internal error: a successful Result cannot become a Result with a value." );
   }

   explicit operator bool() const { return FValue.has_value(); }

   const T& value() const
   {
      Q_ASSERT_X( FValue.has_value(), "Result", "value() of a failed Result" );
      return *FValue;
   }
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
