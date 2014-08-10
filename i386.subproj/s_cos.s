/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

RCSID("$NetBSD: s_cos.S,v 1.7 2001/06/19 00:26:30 fvdl Exp $")

PRIVATE_ENTRY(__cosl)	//currently required for single and double
ENTRY(cosl)
	XMM_ONE_ARG_LONG_DOUBLE_PROLOGUE
	fldt	ARG_LONG_DOUBLE_ONE
	fcos
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
	fcos
	XMM_LONG_DOUBLE_EPILOGUE
	ret

