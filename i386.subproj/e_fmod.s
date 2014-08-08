/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"


RCSID("$NetBSD: e_fmod.S,v 1.6 2001/06/25 16:44:34 fvdl Exp $")

ENTRY(fmod)
	XMM_TWO_ARG_DOUBLE_PROLOGUE
	fldl	ARG_DOUBLE_TWO
	fldl	ARG_DOUBLE_ONE
1:	fprem
	fstsw	%ax
	btw	$10,%ax
	jc	1b
	fstp	%st(1)
	XMM_DOUBLE_EPILOGUE
	ret

ENTRY(fmodf)
	flds	ARG_FLOAT_TWO
	flds	ARG_FLOAT_ONE
1:	fprem
	fstsw	%ax
	btw	$10,%ax
	jc	1b
	fstp	%st(1)
	ret
