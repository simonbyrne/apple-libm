/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*******************************************************************************
*                                                                              *
*      File power.c,                                                           *
*      Function pow ( x, y ),                                                  *
*      Implementation of x^y for the PowerPC.                                  *
*                                                                              *
*      W  A  R  N  I  N  G:  For double precision IEEE machines with MAF       *
*      instructions.                                                           *
*                                                                              *
*      Copyright � 1991-1997 Apple Computer, Inc.  All rights reserved.        *
*                                                                              *
*      Written by Ali Sazegari.                                                *
*                                                                              *
*      June    2  1997 first port of the ibm/taligent power.                   *
*                                                                              *
*******************************************************************************/

#ifdef      __APPLE_CC__
#if         __APPLE_CC__ > 930

#include      "math.h"
#include      "fenv.h"
#include      "fp_private.h"
#include      "fenv_private.h"

#define     POWER_NAN      "37"
#define     SET_INVALID    0x01000000

/*******************************************************************************
*      Floating-point constants.                                               *
*******************************************************************************/

static const double ln2        =  6.931471805599452862e-1;
static const double ln2Tail    =  2.319046813846299558e-17;
static const double kRint      =  6.7553994410557440000e+15; // 0x43380000, 0x00000000
static const double maxExp     =  7.0978271289338397000e+02; // 0x40862e42, 0xfefa39ef
static const double min2Exp    = -7.4513321910194122000e+02; // 0xc0874910, 0xd52d3052
static const double denormal   =  2.9387358770557188000e-39; // 0x37f00000, 0x00000000
static const double twoTo128   =  3.402823669209384635e+38;  // 0x1.0p+128
static const double twoTo52    =  4503599627370496.0;        // 0x1p52
static const double oneOverLn2 =  1.4426950408889633000e+00; // 0x3ff71547, 0x652b82fe
static const hexdouble huge     = HEXDOUBLE(0x7ff00000, 0x00000000);

/*******************************************************************************
*      Approximation coefficients for high precision exp and log.              *
*******************************************************************************/

static const double c2 = -0.5000000000000000042725;
static const double c3 =  0.3333333333296328456505;
static const double c4 = -0.2499999999949632195759;
static const double c5 =  0.2000014541571685319141;
static const double c6 = -0.1666681511199968257150;

static const double d2 = -0.5000000000000000000000;          // bfe0000000000000 
static const double d3 =  0.3333333333333360968212;          // 3FD5555555555587
static const double d4 = -0.2500000000000056849516;          // BFD0000000000066
static const double d5 =  0.1999999996377879912068;          // 3FC9999998D278CB
static const double d6 = -0.1666666662009609743117;          // BFC5555554554F15
static const double d7 =  0.1428690115662276259345;          // 3FC249882224F72F
static const double d8 = -0.1250122079043555957999;          // BFC0006668466517

static const double cc4 = 0.5000000000000000000;
static const double cc3 = 0.16666666666663809522820;
static const double cc2 = 0.04166666666666111110939;
static const double cc1 = 0.008333338095239329810170;
static const double cc0 = 0.001388889583333492938381;

/*******************************************************************************
*      Log and exp table entries definition.                                   *
*******************************************************************************/

extern unsigned long logTable[];
extern unsigned long expTable[];

struct logTableEntry 
      {
      double X;
      double G;
      hexdouble F;
      };

struct expTableEntry 
      {
      double x;
      double f;
      };

#ifdef notdef

static double PowerInner ( double, double );
static double _NearbyInt ( double x ); 

double pow ( double base, double exponent ) 
      {
      register long int isExpOddInt;
      double absBase, result;
      hexdouble u, v, OldEnvironment;
      register unsigned long int expExp;
      
      
      v.d = exponent;
      u.d = base;
      if ( ( ( v.i.hi & 0x000fffff ) | v.i.lo ) == 0 ) 
            {												 // exponent is power of 2 (or +-Inf)
            expExp = v.i.hi & 0xfff00000ul;
            if ( expExp == 0x40000000 ) 
                  return base*base;                          // if exponent == 2.0
            else if ( exponent == 0.0 ) 
                  return 1.0;                                // if exponent == 0.0
            else if ( expExp == 0x3ff00000 ) 
                  return base;                               // if exponent == 1.0
            else if ( expExp == 0xbff00000 ) 
                  return 1.0/base;                           // if exponent == -1.0
            }
            
      if ( ( v.i.hi & 0x7ff00000ul ) < 0x7ff00000ul ) 
            {                                                // exponent is finite
            if ( ( u.i.hi & 0x7ff00000ul ) < 0x7ff00000ul ) 
                  {                                          // base is finite
                  if ( base > 0.0 ) 
                        return PowerInner( base, exponent ); // base is positive
                  else if ( base < 0.0 ) 
                        {
                        if ( _NearbyInt ( exponent ) != exponent )          // exponent is non-integer
							{
                            FEGETENVD( OldEnvironment.d );
							OldEnvironment.i.lo |= SET_INVALID;
							result = nan ( POWER_NAN );
                            FESETENVD( OldEnvironment.d );
							return result;
							}
							
                        result = PowerInner( -base, exponent );
                        if ( _NearbyInt ( 0.5 * exponent ) != 0.5 * exponent ) // exponent is odd
                              result = - result;
                        return ( result );
                        }
                  else 
                        {                                    // base is 0.0:
                        isExpOddInt = ( ( _NearbyInt( exponent ) == exponent ) ? 
                                            ( _NearbyInt( 0.5 * exponent ) != 0.5 * exponent ) : 0 );
                        if ( exponent > 0.0 ) 
                              return ( ( isExpOddInt ) ? base : 0.0 );
                        else                                 // exponent < 0.0
                              return ( ( isExpOddInt ) ? 1.0/base : 1.0/__FABS( base ) );
                        }
                  }
            else if (base != base)
                    return base;
            else
                {                                                // base == +-Inf
                if ( base > 0 ) 
                    return ( exponent > 0 ) ? huge.d : +0.0;
                else 
                    {
                    isExpOddInt = ( ( _NearbyInt ( exponent ) == exponent ) ? 
                                        ( _NearbyInt ( 0.5 * exponent ) != 0.5 * exponent ) : 0 );
                    return ( exponent > 0 ) ? 
                                ( isExpOddInt ? -huge.d : +huge.d ) : ( isExpOddInt ? -0.0 : +0.0 );
                    }
                }
            }
      else if ( exponent != exponent ) 
            return base + exponent;
      else
            {												// exponent is +-Inf
            if ( base != base )
                  return base;
            absBase = __FABS( base );
            if ( ( exponent > 0 && absBase > 1 ) || ( exponent < 0 && absBase < 1 ) )
                  return huge.d;
            else if ( ( exponent > 0 && absBase < 1 ) || ( exponent < 0 && absBase > 1 ) )
                  return +0.0;
            else                                             // now: Abs( base ) == 1.0
                  return 1.0;
            }
      }

/*******************************************************************************
*      Private function _NearbyInt.                                            *
*******************************************************************************/

static double _NearbyInt ( double x ) 
      {
      hexdouble xInHex;
      double OldEnvironment;
      
      xInHex.d = x;
      if ( ( xInHex.i.hi & 0x7ffffffful ) < 0x43300000ul ) 
            {                                                // |x| < 2.0^52
            FEGETENVD( OldEnvironment );               // save environment, set default
            FESETENVD( 0.0 );
            if ( xInHex.i.hi & 0x80000000ul )             // x non-positive
                  x = ( ( x - twoTo52 ) + twoTo52 );
            else
                  x = ( ( x + twoTo52 ) - twoTo52 ); 
            FESETENVD( OldEnvironment );
            return x;
            }
      else                                                   // |x| >= 2.0^52
            return ( x );
      }

/*******************************************************************************
*                                                                              *
*     Function PowerInner computes the base to the exponent power. This        *
*     routine is called internally by the power function. It assumes that      *
*     base is strictly positive, exponent is normal or denormal and the        *
*     rounding direction is round-to-nearest.                                  *
*                                                                              *
*******************************************************************************/
static double PowerInner ( double base, double exponent ) 
      {
      hexdouble u, v, OldEnvironment, scale, exp;
      register long int i, n;
      register unsigned long int head;
      register double z, zLow, high, low, zSquared, temp1, temp2, temp3, result, tail,
                      d, x, xLow, y, yLow, power;
      struct logTableEntry *logTablePointer = ( struct logTableEntry * ) logTable;
      struct expTableEntry *expTablePointer = ( struct expTableEntry * ) expTable + 177;
      
      u.d = base;
      head = u.i.hi;
      
      if ( head >= 0x000ffffful )                            // normal case
            d = ( u.i.hi >> 20 ) - 1023.0;
      else 
            {                                                // denormal case 
            u.d = base * twoTo128;                         // scale to normal range
            d = ( u.i.hi >> 20 ) - 1151.0;
            }

      i = u.i.hi & 0x000fffff;
      u.i.hi = i | 0x3ff00000;

      // Handle exact integral powers and roots of two specially
      if ( ( ( u.i.hi & 0x000fffff ) | u.i.lo )  == 0 )			// base is power of 2
            {
                z = d * exponent;
                if ( z == _NearbyInt( z ) )
                    {
                    // Clip z so conversion to int is safe
                    if (z > 2097)
                        n = 2098;
                    else if (z < -2098)
                        n = -2099;
                    else
                        n = z;

                    return scalbn( 1.0, n );
                    }
            }
            
      if ( u.i.hi & 0x00080000 ) 
            {                                                // 1.5 <= x < 2.0
            n = 1;
            u.d *= 0.5;
            i = ( i >> 13 ) & 0x3f;                          // table lookup index
            }
      else 
            {                                                // 1.0 <= x < 1.5
            n = 0;
            i = ( i >> 12 ) + 64;                            // table lookup index
            }
            
      FEGETENVD( OldEnvironment.d );               // save environment, set default
      FESETENVD( 0.0 );
      
      z = ( u.d - logTablePointer[i].X ) * logTablePointer[i].G;
      zLow = ( ( u.d - logTablePointer[i].X ) - z * logTablePointer[i].X ) * logTablePointer[i].G;
      zSquared = z * z;
      
      if ( n == 0 ) 
            {                                                // n = 0
            v.d = base;
            if ( ( v.i.hi == 0x3ff00000 ) && ( v.i.lo == 0x0 ) ) 
                  {                                          //base = 1.0
                  FESETENVD( OldEnvironment.d );
                  return 1.0;
                  }
            temp1  = d8 * zSquared + d6;
            temp2  = d7 * zSquared + d5;
            temp1  = temp1 * zSquared + d4;
            temp2  = temp2 * zSquared + d3;
            temp1  = temp1 * z + temp2;
            temp2  = temp1 * z + d2;
            temp3  = z + logTablePointer[i].F.d;
            low    = logTablePointer[i].F.d - temp3 + z + ( zLow - z * zLow );
            result = ( temp2 * zSquared + low ) + temp3;
            tail   = temp3 - result + ( temp2 * zSquared + low );
            }
      else if ( logTablePointer[i].F.i.hi != 0 ) 
            {                                                // n = 1
            low    = ln2Tail + zLow;
            high   = z + logTablePointer[i].F.d;
            zLow   = logTablePointer[i].F.d - high + z;
            temp1  = ln2 + low;
            low    = ( ln2 - temp1 ) + low;
            temp3  = c6 * zSquared + c4;
            temp2  = c5 * zSquared + c3;
            temp3  = temp3 * zSquared;
            temp2  = ( temp2 * z + temp3 ) + c2;
            temp3  = high + temp1;
            temp1  = ( temp1 - temp3 ) + high;
            result = ( ( ( temp2 * zSquared + low ) + temp1 ) + zLow ) + temp3;
            tail   = temp3 - result + zLow + temp1 + ( temp2 * zSquared + low );
            }
      else 
            {                                                // n = 1.
            low    = ln2Tail + zLow;
            temp3  = ln2 + low;
            low    = ( ln2 - temp3 ) + low;
            temp1  = d8 * zSquared + d6;
            temp2  = d7 * zSquared + d5;
            temp1  = temp1 * zSquared + d4;
            temp2  = temp2 * zSquared + d3;
            temp1  = temp1 * z + temp2;
            temp2  = temp1 * z + d2;
            result = ( ( temp2 * zSquared + low ) + z ) + temp3;
            tail   = temp3 - result + z + ( temp2 * zSquared + low );
            }
      
      temp1 = d * ln2;
      temp2 = __FMSUB(d, ln2, temp1); // d * ln2 - temp1;
      z     = temp1 + result;
      zLow  = temp1 - z + result + __FMADD(d, ln2Tail, tail + temp2); // ( d * ln2Tail + tail + temp2 );
      temp3 = exponent * zLow; 
      x     = __FMADD(exponent, z, temp3); // exponent * z + temp3;
      xLow  = __FMSUB(exponent, z, x) + temp3; // ( exponent * z - x ) + temp3;
 
      if ( base == _NearbyInt(base) && exponent > 0 && exponent == _NearbyInt(exponent))
      {
        /* NOTHING: +ve integral base and +ve integral exponent should deliver exact result */
      } 
      else
        OldEnvironment.i.lo |= FE_INEXACT;                  // set inexact flag
      
      u.d = x;
      if ( ( u.i.hi & 0x7ff00000ul ) < 0x40800000ul ) 
            {                                                            // abs( x ) < 512
            scale.d = 0.0;
            exp.d = x * oneOverLn2 + kRint;
            scale.i.hi = ( exp.i.lo + 1023 ) << 20;
            exp.d -= kRint;
            y = x - ln2 * exp.d;
            yLow = ln2Tail * exp.d;
            u.d = 512.0 * y + kRint;
            i = u.i.lo;
            d = y - expTablePointer[i].x;
            z = d - yLow;
            zLow = d - z - yLow + xLow;
            zSquared = z * z;
            temp1  = cc0 * zSquared + cc2;
            temp2  = cc1 * zSquared + cc3;
            temp1  = temp1 * zSquared + cc4;
            temp2  = temp1 + temp2 * z;
            temp1  = temp2 * zSquared + z;
            result = scale.d * expTablePointer[i].f;
#if 1
            temp2  = __FMUL( result , ( temp1 + ( zLow + zLow * temp1 ) )); // thwart gcc MAF
#else
            temp2  = result * ( temp1 + ( zLow + zLow * temp1 ) );
#endif
            result += temp2;
            FESETENVD( OldEnvironment.d );
            return result;
            }
      
      if ( ( x <= maxExp ) && ( x > min2Exp ) ) 
            {
            scale.d = 0.0;
            exp.d = x * oneOverLn2 + kRint;
            if ( x >= 512.0 ) 
                  {
                  power = 2.0;
                  scale.i.hi = ( exp.i.lo + 1023 - 1 ) << 20;
                  }
            else 
                  {
                  power = denormal;
                  scale.i.hi = ( exp.i.lo + 1023 + 128 ) << 20;
                  }
            exp.d -= kRint;
            y = x - ln2 * exp.d;
            yLow = ln2Tail * exp.d;
            u.d = 512.0 * y + kRint; 
            i = u.i.lo;
            d = y - expTablePointer[i].x;
            z = d - yLow;
            zLow = d - z - yLow + xLow;
            zSquared = z * z;
            temp1  = cc0 * zSquared + cc2;
            temp2  = cc1 * zSquared + cc3;
            temp1  = temp1 * zSquared + cc4;
            temp2  = temp1 + temp2 * z;
            temp1  = temp2 * zSquared + z;
            result = scale.d * expTablePointer[i].f;
            temp2  = __FMUL( result , ( temp1 + ( zLow + zLow * temp1 ) ) ); // __FMUL thwarts gcc MAF
            u.d  = ( result + temp2 ) * power;
            }
      else if ( x > maxExp ) 
            u.d = huge.d;
      else 
            u.d = +0.0;
      
      i = u.i.hi & 0x7ff00000;
      if ( i == 0x7ff00000 ) 
            OldEnvironment.i.lo |= (FE_OVERFLOW | FE_INEXACT);  // Inf: set overflow/inexact flag
      else if ( i == 0x0 )
            OldEnvironment.i.lo |= (FE_UNDERFLOW | FE_INEXACT); // zero: set underflow/inexact flag
      
      FESETENVD( OldEnvironment.d );
      return u.d;
      }
#else

static double PowerInner ( double, double, hexdouble );
static double _NearbyInt ( double x ); 

double pow ( double base, double exponent ) 
{
      register long int isExpOddInt;
      double absBase, result;
      hexdouble u, v, OldEnvironment;
      register unsigned long int expExp;
      
      register unsigned long GPR_mant, GPR_exp;
      register double FPR_z, FPR_half, FPR_one, FPR_t;
      
      v.d = exponent;										u.d = base;
      GPR_mant = 0x000ffffful;								GPR_exp = 0x7ff00000ul;
      FPR_z = 0.0;											FPR_one = 1.0;
      __ENSURE( FPR_z, FPR_z, FPR_one );
      
      if ( ( ( v.i.hi & GPR_mant ) | v.i.lo ) == 0 ) 
      {														 // exponent is power of 2 (or +-Inf)
            expExp = v.i.hi & 0xfff00000ul;
            if ( expExp == 0x40000000 ) 
                  return base * base;               // if exponent == 2.0
            else if ( exponent == FPR_z ) 
                  return FPR_one;                            // if exponent == 0.0
            else if ( expExp == 0x3ff00000 ) 
                  return base;                               // if exponent == 1.0
            else if ( expExp == 0xbff00000 ) 
                  return FPR_one/base;                       // if exponent == -1.0
      }
            
      if ( ( v.i.hi & GPR_exp ) < GPR_exp ) 
      {                                                		 // exponent is finite
            if ( ( u.i.hi & GPR_exp ) < GPR_exp ) 
            {                                          		 // base is finite
                  if ( base > FPR_z ) 
                        return PowerInner( base, exponent, u ); // base is positive
                  else if ( base < FPR_z ) 
                  {
                        if ( _NearbyInt ( exponent ) != exponent )          // exponent is non-integer
                        {
                            FEGETENVD( OldEnvironment.d );
							OldEnvironment.i.lo |= SET_INVALID;
							result = nan ( POWER_NAN );
                            FESETENVD( OldEnvironment.d );
							return result;
                        }
                        
                        FPR_half = 0.5;
                        u.i.hi ^= 0x80000000;
                        result = PowerInner( -base, exponent, u );
                        FPR_t = FPR_half * exponent;			// XXX undeserved UF INEXACT for tiny exponent XXX
                        if ( _NearbyInt ( FPR_t ) != FPR_t ) // exponent is odd
                              result = - result;
                        return ( result );
                  }
                  else 
                  {                                    		  // base is 0.0:
                        if ( _NearbyInt ( exponent ) == exponent ) // exponent is integral
                        {
                            FPR_half = 0.5;
                            FPR_t = FPR_half * exponent;		// XXX undeserved UF INEXACT for tiny exponent XXX
                            isExpOddInt = ( _NearbyInt ( FPR_t ) != FPR_t );
                        }
                        else
                            isExpOddInt = 0;
                            
                        if ( exponent > FPR_z ) 
                              return ( ( isExpOddInt ) ? base : FPR_z );
                        else                                 // exponent < 0.0
                              return ( ( isExpOddInt ) ? FPR_one/base : FPR_one/__FABS( base ) );
                  }
            }
            else if (base != base)
                    return base;
            else
            {                                                // base == +-Inf
                if ( base > FPR_z ) 
                    return ( exponent > FPR_z ) ? huge.d : FPR_z;
                else 
                {
                    if ( _NearbyInt ( exponent ) == exponent ) // exponent is integral
                    {
                        FPR_half = 0.5;
                        FPR_t = FPR_half * exponent;		// XXX undeserved UF INEXACT for tiny exponent XXX
                        isExpOddInt = ( _NearbyInt ( FPR_t ) != FPR_t );
                    }
                    else
                        isExpOddInt = 0;
                    return ( exponent > 0 ) ? 
                                ( isExpOddInt ? -huge.d : +huge.d ) : ( isExpOddInt ? -FPR_z : FPR_z );
                }
            }
      }
      else if ( exponent != exponent ) 
            return base + exponent;
      else
      {														// exponent is +-Inf
            if ( base != base )
                  return base;
            absBase = __FABS( base );
            if ( ( exponent > FPR_z && absBase > FPR_one ) || ( exponent < FPR_z && absBase < FPR_one ) )
                  return huge.d;
            else if ( ( exponent > FPR_z && absBase < FPR_one ) || ( exponent < FPR_z && absBase > FPR_one ) )
                  return FPR_z;
            else                                             // now: Abs( base ) == 1.0
                  return FPR_one;
      }
}

/*******************************************************************************
*      Private function _NearbyInt.                                            *
*******************************************************************************/

// If x is integral, returns x.
// If x is not an integer, returns either floor(x) or ceil(x) (i.e. some vaule
// different than x). On this basis, _NearbyInt can be used to detect integral x.
// Arithmetic could raise INEXACT, so protect callers flags.

static double _NearbyInt ( double x ) 
{
      register double result, FPR_env, FPR_absx, FPR_Two52, FPR_z;
      
      FPR_absx = __FABS( x );
      FPR_z = 0.0;											FPR_Two52 = twoTo52;
      
      FEGETENVD( FPR_env );
      __ENSURE( FPR_z, FPR_Two52, FPR_z );
      
      if ( FPR_absx < FPR_Two52 )							// |x| < 2.0^52
      {
            FESETENVD( FPR_z );
            if ( x < FPR_z )
                  result = ( ( x - FPR_Two52 ) + FPR_Two52 );
            else
                  result = ( ( x + FPR_Two52 ) - FPR_Two52 ); 
            FESETENVD( FPR_env );
            return result;
      }
      else                                                   // |x| >= 2.0^52
            return ( x );
}

// Called when our environment is installed (and callers is stashed safely aside).
static inline double _NearbyIntDfltEnv ( double x ) 
{
      register double result, FPR_absx, FPR_Two52, FPR_z;
      
      FPR_absx = __FABS( x );
      FPR_z = 0.0;											FPR_Two52 = twoTo52;
      
      __ENSURE( FPR_z, FPR_Two52, FPR_z );
      
      if ( FPR_absx < FPR_Two52 )							// |x| < 2.0^52
      {
            if ( x < FPR_z )
                  result = ( ( x - FPR_Two52 ) + FPR_Two52 );
            else
                  result = ( ( x + FPR_Two52 ) - FPR_Two52 ); 
            return result;
      }
      else                                                   // |x| >= 2.0^52
            return ( x );
}

static const double kMinNormal = 2.2250738585072014e-308;  // 0x1.0p-1022
static const double kMaxNormal = 1.7976931348623157e308;
static const hexdouble kConvULDouble     = HEXDOUBLE(0x43300000, 0x00000000); // for *unsigned* to double

static double PowerInner ( double base, double exponent, hexdouble u ) 
{
      hexdouble dInHex, scale, exp;
      register long int i, n;
      register double z, zLow, high, low, zSquared, temp1, temp2, temp3, result, tail,
                      d, x, xLow, y, yLow, power;
      struct logTableEntry *logTablePointer = ( struct logTableEntry * ) logTable;
      struct expTableEntry *expTablePointer = ( struct expTableEntry * ) expTable + 177;
      
      register double FPR_env, FPR_z, FPR_half, FPR_one, FPR_512, FPR_t, FPR_G, FPR_X, FPR_ud;
      register double FPR_q, FPR_kConvDouble;
      register struct logTableEntry *pT;
      register struct expTableEntry *pE;
      
      FPR_z = 0.0;											FPR_one = 1.0;
      FPR_half = 0.5;										FPR_512 = 512.0;
      FPR_kConvDouble = kConvULDouble.d;					dInHex.i.hi = 0x43300000; 	// store upper half
         
      if ( base == FPR_one ) 
            return FPR_one;
    
      if ( u.i.hi >= 0x000ffffful )                  		// normal case
      {
            FPR_q = 1023.0;
      }
      else 
      {                                                		// denormal case             
            u.d = __FMUL( base, twoTo128 );                 // scale to normal range
            FPR_q = 1151.0;
      }

      dInHex.i.lo = u.i.hi >> 20; 							// store lower half

      i = u.i.hi & 0x000fffff;
      u.i.hi = i | 0x3ff00000;

      FEGETENVD( FPR_env);               					// save environment, set default
      __ENSURE( FPR_z, FPR_half, FPR_512 );					__ENSURE( FPR_z, FPR_half, FPR_one );
      FESETENVD( FPR_z );

      // Handle exact integral powers and roots of two specially
      if ( 0 == (( u.i.hi & 0x000fffff ) | u.i.lo) )		// base is power of 2
      {
            d = dInHex.d;									// float load double
            d -= FPR_kConvDouble; 							// subtract magic value
            d -= FPR_q;

            z = __FMUL( d, exponent );
            if ( z == _NearbyIntDfltEnv( z ) )
            {
                // Clip z so conversion to int is safe
                if (z > 2097.0)
                    n = 2098;
                else if (z < -2098.0)
                    n = -2099;
                else
                    n = z;									// The common case. Emits fctiwz, stfd

                FESETENVD( FPR_env );
                return scalbn( FPR_one, n );				// lwz "n" -- make sure this is on cycle following stfd!
            }
      }
            
      if ( u.i.hi & 0x00080000 ) 
      {                                                		// 1.5 <= x < 2.0
            n = 1;
            i = ( i >> 13 ) & 0x3f;                         // table lookup index
            FPR_ud = __FMUL( u.d, FPR_half );
            u.d = FPR_ud;
      }
      else 
      {                                                		// 1.0 <= x < 1.5
            n = 0;
            i = ( i >> 12 ) + 64;                           // table lookup index
            FPR_ud = u.d;
      }
            
      pT = &(logTablePointer[i]);
      
      FPR_X = pT->X;										FPR_G = pT->G;
      FPR_t = FPR_ud - FPR_X;
      
      z = __FMUL( FPR_t, FPR_G );
      
      zSquared = __FMUL( z, z ); 							FPR_t = __FNMSUB( z, FPR_X, FPR_t );
      zLow = __FMUL( FPR_t, FPR_G );
      
      
      if ( n == 0 ) 
      {                                                		// n = 0
            register double FPR_Fd, FPR_d2, FPR_d3, FPR_d4, FPR_d5, FPR_d6, FPR_d7, FPR_d8;
                        
            FPR_d8 = d8;									FPR_d6 = d6;

            FPR_d7 = d7;									FPR_d5 = d5;
            
            FPR_Fd = pT->F.d; 

            temp1 = __FMADD( FPR_d8, zSquared, FPR_d6);		temp2 = __FMADD( FPR_d7, zSquared, FPR_d5);
            FPR_d4 = d4;								
            
            FPR_d3 = d3;
            temp3 = z + FPR_Fd;								FPR_t = __FNMSUB( z, zLow, zLow );
            
            temp1 = __FMADD( temp1, zSquared, FPR_d4);		temp2 = __FMADD( temp2, zSquared, FPR_d3);
            FPR_d2 = d2;								
            
            temp1 = __FMADD( temp1, z, temp2);				low = FPR_Fd - temp3 + z + FPR_t;		

            temp2 = __FMADD( temp1, z, FPR_d2);			
            
            FPR_t = __FMADD( temp2, zSquared, low );
            
            result = FPR_t + temp3;
            
            tail = temp3 - result + FPR_t;
      }
      else if ( pT->F.i.hi != 0 ) 
      {                                                		// n = 1
            register double FPR_Fd, FPR_ln2, FPR_ln2Tail, FPR_c2, FPR_c3, FPR_c4, FPR_c5, FPR_c6;
                        
            FPR_c6 = c6;									FPR_c4 = c4;
            
            FPR_c5 = c5;									FPR_c3 = c3;
            
            temp3 = __FMADD( FPR_c6, zSquared, FPR_c4 );	temp2  = __FMADD( FPR_c5, zSquared, FPR_c3);
            FPR_ln2Tail = ln2Tail;
            
            FPR_ln2 = ln2;									FPR_Fd = pT->F.d;
            low = FPR_ln2Tail + zLow;					
            
            high   = z + FPR_Fd;							temp3 = __FMUL( temp3, zSquared );
            FPR_c2 = c2;								
                        
            zLow = FPR_Fd - high + z;						temp1  = FPR_ln2 + low;
                        
            temp2 = __FMADD( temp2, z, temp3 ) + FPR_c2;	low = ( FPR_ln2 - temp1 ) + low;
            
            temp3  = high + temp1;
            
            temp1  = ( temp1 - temp3 ) + high;
            
            FPR_t = __FMADD( temp2, zSquared, low );
            
            result = ( ( FPR_t + temp1 ) + zLow ) + temp3;
            
            tail   = temp3 - result + zLow + temp1 + FPR_t;
      }
      else 
      {                                                // n = 1.
            register double FPR_ln2, FPR_ln2Tail, FPR_d2, FPR_d3, FPR_d4, FPR_d5, FPR_d6, FPR_d7, FPR_d8;
                        
            FPR_d8 = d8;									FPR_d6 = d6;
            
            FPR_d7 = d7;									FPR_d5 = d5;
            
            temp1 = __FMADD( FPR_d8, zSquared, FPR_d6);		temp2 = __FMADD( FPR_d7, zSquared, FPR_d5);
            FPR_ln2Tail = ln2Tail;
            
            FPR_ln2 = ln2;
            low    = FPR_ln2Tail + zLow;					__ORI_NOOP;
            
            FPR_d4 = d4;									FPR_d3 = d3;
            
            temp1 = __FMADD( temp1, zSquared, FPR_d4);		temp2 = __FMADD( temp2, zSquared, FPR_d3);
            FPR_d2 = d2;								
            
            temp1 = __FMADD( temp1, z, temp2);				temp3  = FPR_ln2 + low;

            temp2 = __FMADD( temp1, z, FPR_d2);				low    = ( FPR_ln2 - temp3 ) + low;
                                    
            FPR_t = __FMADD( temp2, zSquared, low );
            
            result = ( FPR_t + z ) + temp3;
            
            tail   = temp3 - result + z + FPR_t;
      }
      
      {
            register double FPR_ln2, FPR_ln2Tail;
            
            d = dInHex.d;									// float load double
            d -= FPR_kConvDouble; 							// subtract magic value
            d -= FPR_q;
            
            FPR_ln2 = ln2;									FPR_ln2Tail = ln2Tail;
            
            temp1 = __FMUL( d, FPR_ln2 );						
            
            temp2 = __FMSUB( d, FPR_ln2, temp1 ); 			z = temp1 + result;
            
            FPR_t = tail + temp2;						
            
            zLow = temp1 - z + result + __FMADD(d, FPR_ln2Tail, FPR_t); 
            
            temp3 = exponent * zLow;                   

            x = __FMADD( exponent, z, temp3 );
            
            xLow = __FMSUB( exponent, z, x ) + temp3;
      }
       
      if ( __FABS( x ) < FPR_512 ) 
      {                                                    	// abs( x ) < 512
            register double FPR_ln2, FPR_ln2Tail, FPR_ln2Inv, FPR_kRint, FPR_f;
            register double FPR_cc0, FPR_cc1, FPR_cc2, FPR_cc3, FPR_cc4;
            
            FPR_ln2 = ln2;									FPR_ln2Tail = ln2Tail;
            FPR_ln2Inv = oneOverLn2;						FPR_kRint = kRint;
            
            FPR_t = __FMADD( x, FPR_ln2Inv, FPR_kRint );
            exp.d = FPR_t;

            FPR_t -= FPR_kRint;
            
            y = __FNMSUB( FPR_ln2, FPR_t, x);				yLow = __FMUL( FPR_ln2Tail, FPR_t );
            
            u.d = __FMADD( FPR_512, y, FPR_kRint );
            
            scale.i.lo = 0;
            scale.i.hi = ( exp.i.lo + 1023 ) << 20;
            
            FPR_cc0 = cc0;									FPR_cc2 = cc2;
            FPR_cc1 = cc1;									FPR_cc3 = cc3;
            FPR_cc4 = cc4;								
     
            i = u.i.lo;
            
            pE = &(expTablePointer[i]);
            
            d = y - pE->x;
            z = d - yLow;
            
            zSquared = __FMUL( z, z );						zLow = d - z - yLow + xLow;
            
            FPR_t = scale.d;								FPR_f = pE->f;
            
            temp1  = __FMADD( FPR_cc0, zSquared, FPR_cc2 );	temp2  = __FMADD( FPR_cc1, zSquared, FPR_cc3 );
            
            temp1  = __FMADD( temp1, zSquared, FPR_cc4 );
            
            temp2  = __FMADD( temp2, z, temp1 );
            
            temp1  = __FMADD( temp2, zSquared, z );			result = __FMUL( FPR_t, FPR_f );

            temp2  = __FMUL( result , ( temp1 + ( zLow + zLow * temp1 ) )); // __FMUL thwarts gcc MAF

            result += temp2;
            
            if ( exponent > FPR_z && base == _NearbyIntDfltEnv(base) && exponent == _NearbyIntDfltEnv(exponent))
            {
                FESETENVD( FPR_env ); /* NOTHING: +ve integral base and +ve integral exponent deliver exact result */
            }
            else
            {
                FESETENVD( FPR_env );
                __PROG_INEXACT ( FPR_ln2 );
            }
                
            return result;
      }
      
      if ( ( x <= maxExp ) && ( x > min2Exp ) ) 
      {
            register double FPR_ln2, FPR_ln2Tail, FPR_ln2Inv, FPR_kRint, FPR_f;
            register double FPR_cc0, FPR_cc1, FPR_cc2, FPR_cc3, FPR_cc4;
            
            FPR_ln2 = ln2;									FPR_ln2Tail = ln2Tail;
            FPR_ln2Inv = oneOverLn2;						FPR_kRint = kRint;
            
            FPR_t = __FMADD( x, FPR_ln2Inv, FPR_kRint );
            exp.d = FPR_t;
            
            if ( x >= FPR_512 ) 
            {
                  power = 2.0;
                  scale.i.lo = 0;
                  scale.i.hi = ( exp.i.lo + 1023 - 1 ) << 20;
            }
            else 
            {
                  power = denormal;
                  scale.i.lo = 0;
                  scale.i.hi = ( exp.i.lo + 1023 + 128 ) << 20;
            }
            
            FPR_t -= FPR_kRint;
            exp.d = FPR_t;
            
            y = __FNMSUB( FPR_ln2, FPR_t, x);				yLow = __FMUL( FPR_ln2Tail, FPR_t );
            
            u.d = __FMADD( FPR_512, y, FPR_kRint );
            
            FPR_cc0 = cc0;									FPR_cc2 = cc2;
            FPR_cc1 = cc1;									FPR_cc3 = cc3;
            FPR_cc4 = cc4;								
            
            i = u.i.lo;
            
            pE = &(expTablePointer[i]);
            
            d = y - pE->x;
            z = d - yLow;
            
            zSquared = __FMUL( z, z );						zLow = d - z - yLow + xLow;
 
            FPR_t = scale.d;								FPR_f = pE->f;
            
            temp1  = __FMADD( FPR_cc0, zSquared, FPR_cc2 );	temp2  = __FMADD( FPR_cc1, zSquared, FPR_cc3 );
            
            temp1  = __FMADD( temp1, zSquared, FPR_cc4 );
            
            temp2  = __FMADD( temp2, z, temp1 );
            
            temp1  = __FMADD( temp2, zSquared, z );
            
            result = __FMUL( FPR_t, FPR_f );
            
            temp2  = __FMUL( result , ( temp1 + ( zLow + zLow * temp1 ) ) ); // __FMUL thwarts gcc MAF
            
            result  = ( result + temp2 ) * power;
      }
      else if ( x > maxExp ) 
            result = huge.d;
      else 
            result = FPR_z;
      
      FESETENVD( FPR_env );
      if ( __FABS( result ) == huge.d ) 
            __PROG_OF_INEXACT( kMaxNormal );  				// Inf: set overflow/inexact flag
      else if ( result == FPR_z )
            __PROG_UF_INEXACT( kMinNormal ); 				// zero: set underflow/inexact flag
      else
            __PROG_INEXACT ( ln2 );
      
      return result;
}
#endif

#else       /* __APPLE_CC__ version */
#error Version gcc-932 or higher required.  Compilation terminated.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */
