/*
 * dremf() wrapper for remainderf().
 *
 * Written by J.T. Conklin, <jtc@wimsey.com>
 * Placed into the Public Domain, 1994.
 */

#include <sys/cdefs.h>
#if defined(LIBM_SCCS) && !defined(lint)
__RCSID("$NetBSD: w_dremf.c,v 1.3 1999/07/02 15:37:44 simonb Exp $");
#endif

#include "math.h"
#include "math_private.h"

float
dremf(x, y)
	float x, y;
{
	return remainderf(x, y);
}
