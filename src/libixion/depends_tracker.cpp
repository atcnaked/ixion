/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "depends_tracker.hpp"

#include "ixion/global.hpp"
#include "ixion/cell.hpp"
#include "ixion/cell_queue_manager.hpp"
#include "ixion/formula_name_resolver.hpp"

#include "ixion/interface/formula_model_access.hpp"

#include <vector>
#include <iostream>
#include <fstream>

using namespace std;

namespace ixion {

namespace {

/**
 * Function object to reset the status of formula cell to pre-interpretation
 * status.
 */
class cell_reset_handler : public unary_function<abs_address_t, void>
{
    iface::formula_model_access& m_cxt;
public:
    cell_reset_handler(iface::formula_model_access& cxt) : m_cxt(cxt) {}
    void operator() (const abs_address_t& pos) const
    {
        formula_cell* p = m_cxt.get_formula_cell(pos);
        p->reset();
    }
};

class circular_check_handler : public unary_function<abs_address_t, void>
{
    iface::formula_model_access& m_cxt;
public:
    circular_check_handler(iface::formula_model_access& cxt) : m_cxt(cxt) {}

    void operator() (const abs_address_t& pos) const
    {
        formula_cell* p = m_cxt.get_formula_cell(pos);
        p->check_circular(m_cxt, pos);
    }
};

class thread_queue_handler : public unary_function<abs_address_t, void>
{
    iface::formula_model_access& m_cxt;
public:
    thread_queue_handler(iface::formula_model_access& cxt) : m_cxt(cxt) {}
    void operator() (const abs_address_t& pos) const
    {
        cell_queue_manager::add_cell(pos);
    }
};

struct cell_interpret_handler : public unary_function<abs_address_t, void>
{
    cell_interpret_handler(iface::formula_model_access& cxt) :
        m_context(cxt) {}

    void operator() (const abs_address_t& pos) const
    {
        formula_cell* p = m_context.get_formula_cell(pos);
        p->interpret(m_context, pos);
    }
private:
    iface::formula_model_access& m_context;
};

}

dependency_tracker::cell_back_inserter::cell_back_inserter(vector<abs_address_t> & sorted_cells) :
    m_sorted_cells(sorted_cells) {}

void dependency_tracker::cell_back_inserter::operator() (const abs_address_t& cell)
{
    m_sorted_cells.push_back(cell);
}

// ============================================================================

dependency_tracker::dependency_tracker(
    const dirty_formula_cells_t& dirty_cells, iface::formula_model_access& cxt) :
    m_dirty_cells(dirty_cells), m_context(cxt)
{
}

dependency_tracker::~dependency_tracker()
{
}

void dependency_tracker::insert_depend(const abs_address_t& origin_cell, const abs_address_t& depend_cell)
{
    m_deps.insert(origin_cell, depend_cell);
}

void dependency_tracker::interpret_all_cells(size_t thread_count)
{
    vector<abs_address_t> sorted_cells;
    topo_sort_cells(sorted_cells);

    // Reset cell status.
    for_each(sorted_cells.begin(), sorted_cells.end(), cell_reset_handler(m_context));

    // First, detect circular dependencies and mark those circular
    // dependent cells with appropriate error flags.
    for_each(sorted_cells.begin(), sorted_cells.end(), circular_check_handler(m_context));

    if (thread_count > 0)
    {
        // Interpret cells in topological order using threads.
        cell_queue_manager::init(thread_count, m_context);
        for_each(sorted_cells.begin(), sorted_cells.end(), thread_queue_handler(m_context));
        cell_queue_manager::terminate();
    }
    else
    {
        // Interpret cells using just a single thread.
        for_each(sorted_cells.begin(), sorted_cells.end(), cell_interpret_handler(m_context));
    }
}

void dependency_tracker::topo_sort_cells(vector<abs_address_t>& sorted_cells) const
{
    cell_back_inserter handler(sorted_cells);
    vector<abs_address_t> all_cells;
    all_cells.reserve(m_dirty_cells.size());
    dirty_formula_cells_t::const_iterator itr = m_dirty_cells.begin(), itr_end = m_dirty_cells.end();
    for (; itr != itr_end; ++itr)
        all_cells.push_back(*itr);

    dfs_type dfs(all_cells, m_deps.get(), handler);
    dfs.run();
}

}
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
