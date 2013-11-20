
/*
 *  vabs~
 *
 *	vabs~ is a vectorised version of abs~.
 *
 *  Copyright 2010 Alex Harker. All rights reserved.
 *
 */


#include <AH_VectorOps.h>


// Object and function naming

#define OBJNAME_STR "vabs~"
#define OBJNAME_FIRST(a) vabs ## a
#define OBJNAME_SECOND(a) a ## vabs

// Constants

vFloat v_bit_mask_32;

unsigned long long bit_mask_64 = 0x7FFFFFFFFFFFFFFFULL; 
unsigned long bit_mask_32 = 0x7FFFFFFFUL;

#ifdef VECTOR_F64_128BIT
vDouble v_bit_mask_64; 
#define SET_CONSTANTS										\
v_bit_mask_64 = double2vector(*(double *)&bit_mask_64);		\
v_bit_mask_32 = float2vector(*(float *)&bit_mask_32);
#else
#define SET_CONSTANTS										\
v_bit_mask_32 = float2vector(*(float *)&bit_mask_32);
#endif

// Core functions (all intrinsic-based)

__inline float abs_scalar_32 (float in)
{
	unsigned long temp = (*(unsigned long *) &in) & 0x7FFFFFFF; 
	return *(float *) &temp;
}

__inline double abs_scalar_64 (double in)
{
	unsigned long long temp = (*(unsigned long long*) &in) & 0x7FFFFFFFFFFFFFFFULL; 
	return *(double *) &temp;
}

// Define operations (all intrinsic-based)

#define F32_VEC_OP(a) F32_VEC_AND_OP(a, v_bit_mask_32)
#define F32_SCALAR_OP abs_scalar_32

#define F64_VEC_OP(a) F64_VEC_AND_OP(a, v_bit_mask_64)
#define F64_SCALAR_OP abs_scalar_64

// Having defined the necessary constants and macro the bulk of the code can now be included

#include "Template_Unary.h"
