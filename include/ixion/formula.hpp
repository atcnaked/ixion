/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_IXION_FORMULA_HPP
#define INCLUDED_IXION_FORMULA_HPP

#include "ixion/formula_tokens.hpp"
#include "ixion/interface/formula_model_access.hpp"
#include "ixion/env.hpp"

#include <string>

namespace ixion {

class formula_name_resolver;

/**
 * Parse a raw formula expression string into formula tokens.
 *
 * @param cxt model context.
 * @param pos address of the cell that has the formula expression.
 * @param resolver name resolver object used to resolve name tokens.
 * @param p pointer to the first character of raw formula expression string.
 * @param n size of the raw formula expression string.
 *
 * @return formula tokens representing the parsed formula expression.
 */
IXION_DLLPUBLIC formula_tokens_t parse_formula_string(
    iface::formula_model_access& cxt, const abs_address_t& pos,
    const formula_name_resolver& resolver, const char* p, size_t n);

/**
 * Convert formula tokens into a human-readable string representation.
 *
 * @param cxt model context.
 * @param pos address of the cell that has the formula tokens.
 * @param resolver name resolver object used to print name tokens.
 * @param tokens formula tokens.
 *
 * @return string representation of the formula tokens.
 */
IXION_DLLPUBLIC std::string print_formula_tokens(
    const iface::formula_model_access& cxt, const abs_address_t& pos,
    const formula_name_resolver& resolver, const formula_tokens_t& tokens);

/**
 * Regisiter a formula cell with cell dependency tracker.
 *
 * @param cxt model context.
 * @param pos address of the cell being registered.  In case of grouped
 *            cells, the position must be that of teh top-left cell of that
 *            group.
 */
void IXION_DLLPUBLIC register_formula_cell(
    iface::formula_model_access& cxt, const abs_address_t& pos);

/**
 * Unregister a formula cell with cell dependency tracker if a formula cell
 * exists at specified cell address.  If there is no existing cell at the
 * specified address, or the cell is not a formula cell, this function is a
 * no-op.
 *
 * @param cxt model context.
 * @param pos address of the cell being unregistered.
 */
void IXION_DLLPUBLIC unregister_formula_cell(
    iface::formula_model_access& cxt, const abs_address_t& pos);

/**
 * Get the positions of those formula cells that directly or indirectly
 * depend on the specified source cells.
 *
 * @param cxt model context.
 * @param modified_cells collection of the postiions of cells that have been
 *                       modified.
 *
 * @return collection of the positions of formula cells that directly or
 *         indirectly depend on at least one of the specified source cells.
 */
IXION_DLLPUBLIC abs_address_set_t query_dirty_cells(
    iface::formula_model_access& cxt, const abs_address_set_t& modified_cells);

/**
 * Calculate all dirty cells in order of dependency.
 *
 * @param cxt model context.
 * @param formula_cells all formula cells to be calculated.
 * @param thread_count number of calculation threads to use.  Note that
 *                     passing 0 will make the process use the main thread
 *                     only, while passing any number greater than 0 will
 *                     make the process spawn specified number of
 *                     calculation threads plus one additional thread to
 *                     manage the calculation threads.
 */
void IXION_DLLPUBLIC calculate_cells(
    iface::formula_model_access& cxt, abs_address_set_t& formula_cells, size_t thread_count);

}

#endif
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
