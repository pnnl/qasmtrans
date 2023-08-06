#pragma once
/*****************************************************
 * Meta definition of QASMTrans, serves as main header
 ****************************************************/
#ifndef QASMTRANS_HPP
#define QASMTRANS_HPP

/***********************************************
 * Constant configuration:
 ***********************************************/
/* Constant value of PI */
#define PI 3.14159265358979323846
/* Constant value of sqrt(2) */
#define S2I 0.70710678118654752440
/* Constant value of 0.5 */
#define HALF 0.5
/* Error bar for purity check and other error check */
#define ERROR_BAR (1e-3)
#define PRINT_PROGRESS_BAR

namespace QASMTrans
{
    /* Basic data type for indices */
    using IdxType = long long int;
    /* Basic data type for value */
    using ValType = double;

}
#endif
