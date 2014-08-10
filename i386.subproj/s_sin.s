/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

RCSID("$NetBSD: s_sin.S,v 1.6 2001/06/19 00:26:31 fvdl Exp $")


PRIVATE_ENTRY(__sinl) //not public. Currently used by singled and double precision entry points
ENTRY(sinl)
	XMM_ONE_ARG_LONG_DOUBLE_PROLOGUE
	fldt	ARG_LONG_DOUBLE_ONE
	fsin
	fnstsw	%ax
	andw	$0x400,%ax
	jnz	1f
	XMM_LONG_DOUBLE_EPILOGUE
	ret
1:	fldpi
	fadd	%st(0)
	fxch	%st(1)
2:	fprem1
	fnstsw	%ax
	andw	$0x400,%ax
	jnz	2b
	fstp	%st(1)
	fsin
	XMM_LONG_DOUBLE_EPILOGUE
	ret
