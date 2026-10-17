#include "Result.h"

#include <gtest/gtest.h>

namespace {

Result<int> half( int aValue )
{
   if ( aValue % 2 != 0 )
      return Error{ QStringLiteral( "%1 is odd" ).arg( aValue ) };
   return aValue / 2;
}

Result<> needsPositive( int aValue )
{
   if ( aValue <= 0 )
      return Error{ QStringLiteral( "not positive" ) };
   return {};
}

TEST( Result, ValueOrError )
{
   const Result<int> ok = half( 8 );
   ASSERT_TRUE( ok );
   EXPECT_EQ( ok.value(), 4 );
   EXPECT_TRUE( ok.error().isEmpty() );

   const Result<int> bad = half( 7 );
   EXPECT_FALSE( bad );
   EXPECT_EQ( bad.error(), QStringLiteral( "7 is odd" ) );
}

// A zero value is a success: the old "-1 or 0" convention could not tell these apart.
TEST( Result, ZeroIsNotFailure )
{
   const Result<int> zero = half( 0 );
   ASSERT_TRUE( zero );
   EXPECT_EQ( zero.value(), 0 );
}

TEST( Result, VoidSpecialisation )
{
   EXPECT_TRUE( needsPositive( 1 ) );

   const Result<> bad = needsPositive( 0 );
   EXPECT_FALSE( bad );
   EXPECT_EQ( bad.error(), QStringLiteral( "not positive" ) );

   // An Error with no message is still a failure.
   const Result<> silent = Error{};
   EXPECT_FALSE( silent );
}

// A failure passes up through any Result type as `return failed;`.
Result<int> passesUp( int aValue )
{
   if ( const Result<> positive = needsPositive( aValue ); !positive )
      return positive;
   if ( const Result<int> halved = half( aValue ); !halved )
      return halved;
   return aValue;
}

TEST( Result, FailureConvertsBetweenTypes )
{
   const Result<int> notPositive = passesUp( 0 );
   EXPECT_FALSE( notPositive );
   EXPECT_EQ( notPositive.error(), QStringLiteral( "not positive" ) );

   const Result<int> odd = passesUp( 3 );
   EXPECT_FALSE( odd );
   EXPECT_EQ( odd.error(), QStringLiteral( "3 is odd" ) );

   const Result<> dropped = half( 8 );  // a success keeps being one when the value is dropped
   EXPECT_TRUE( dropped );
   const Result<> droppedFailure = half( 7 );
   EXPECT_FALSE( droppedFailure );
   EXPECT_EQ( droppedFailure.error(), QStringLiteral( "7 is odd" ) );
}

// Debug builds assert instead.
#ifdef QT_NO_DEBUG
TEST( Result, SuccessConvertedToAValueResultFailsWithAMessage )
{
   const Result<> succeeded;
   const Result<int> converted = succeeded;
   EXPECT_FALSE( converted );
   EXPECT_FALSE( converted.error().isEmpty() );
}
#endif

}  // namespace
