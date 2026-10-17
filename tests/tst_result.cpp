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

}  // namespace
