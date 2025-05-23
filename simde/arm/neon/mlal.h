/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2020      Evan Nemerson <evan@nemerson.com>
 *   2020      Sean Maher <seanptmaher@gmail.com> (Copyright owned by Google, LLC)
 */

#if !defined(SIMDE_ARM_NEON_MLAL_H)
#define SIMDE_ARM_NEON_MLAL_H

#include "movl.h"
#include "mla.h"
#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t simde_vmlal_s8(simde_int16x8_t a, simde_int8x8_t b, simde_int8x8_t c)
{
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmlal_s8(a, b, c);
#else
    return simde_vmlaq_s16(a, simde_vmovl_s8(b), simde_vmovl_s8(c));
#endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
#undef vmlal_s8
#define vmlal_s8(a, b, c) simde_vmlal_s8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t simde_vmlal_s16(simde_int32x4_t a, simde_int16x4_t b, simde_int16x4_t c)
{
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmlal_s16(a, b, c);
#else
    return simde_vmlaq_s32(a, simde_vmovl_s16(b), simde_vmovl_s16(c));
#endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
#undef vmlal_s16
#define vmlal_s16(a, b, c) simde_vmlal_s16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t simde_vmlal_s32(simde_int64x2_t a, simde_int32x2_t b, simde_int32x2_t c)
{
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmlal_s32(a, b, c);
#else
    simde_int64x2_private r_, a_ = simde_int64x2_to_private(a),
                              b_ = simde_int64x2_to_private(simde_vmovl_s32(b)),
                              c_ = simde_int64x2_to_private(simde_vmovl_s32(c));

#if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
    r_.values = (b_.values * c_.values) + a_.values;
#else
    SIMDE_VECTORIZE
    for (size_t i = 0; i < (sizeof(r_.values) / sizeof(r_.values[0])); i++) {
        r_.values[i] = (b_.values[i] * c_.values[i]) + a_.values[i];
    }
#endif

    return simde_int64x2_from_private(r_);
#endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
#undef vmlal_s32
#define vmlal_s32(a, b, c) simde_vmlal_s32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t simde_vmlal_u8(simde_uint16x8_t a, simde_uint8x8_t b, simde_uint8x8_t c)
{
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmlal_u8(a, b, c);
#else
    return simde_vmlaq_u16(a, simde_vmovl_u8(b), simde_vmovl_u8(c));
#endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
#undef vmlal_u8
#define vmlal_u8(a, b, c) simde_vmlal_u8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t simde_vmlal_u16(simde_uint32x4_t a, simde_uint16x4_t b, simde_uint16x4_t c)
{
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmlal_u16(a, b, c);
#else
    return simde_vmlaq_u32(a, simde_vmovl_u16(b), simde_vmovl_u16(c));
#endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
#undef vmlal_u16
#define vmlal_u16(a, b, c) simde_vmlal_u16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t simde_vmlal_u32(simde_uint64x2_t a, simde_uint32x2_t b, simde_uint32x2_t c)
{
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmlal_u32(a, b, c);
#else
    simde_uint64x2_private r_, a_ = simde_uint64x2_to_private(a),
                               b_ = simde_uint64x2_to_private(simde_vmovl_u32(b)),
                               c_ = simde_uint64x2_to_private(simde_vmovl_u32(c));

#if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
    r_.values = (b_.values * c_.values) + a_.values;
#else
    SIMDE_VECTORIZE
    for (size_t i = 0; i < (sizeof(r_.values) / sizeof(r_.values[0])); i++) {
        r_.values[i] = (b_.values[i] * c_.values[i]) + a_.values[i];
    }
#endif

    return simde_uint64x2_from_private(r_);
#endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
#undef vmlal_u32
#define vmlal_u32(a, b, c) simde_vmlal_u32((a), (b), (c))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_MLAL_H) */
