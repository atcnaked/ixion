/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ixion/formula_function_opcode.hpp"

#include "formula_functions.hpp"

namespace ixion {

const char* get_formula_function_name(formula_function_t func)
{
    return formula_functions::get_function_name(func);
}

formula_function_t get_formula_function_opcode(const char* p, size_t n)
{
    return formula_functions::get_function_opcode(p, n);
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
