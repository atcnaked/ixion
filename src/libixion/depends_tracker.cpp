/*************************************************************************
 *
 * Copyright (c) 2010, 2011 Kohei Yoshida
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 ************************************************************************/

#include "ixion/depends_tracker.hpp"
#include "ixion/global.hpp"
#include "ixion/cell.hpp"
#include "ixion/formula_interpreter.hpp"
#include "ixion/cell_queue_manager.hpp"
#include "ixion/hash_container/map.hpp"

#include "ixion/interface/model_context.hpp"

#include <vector>
#include <iostream>
#include <fstream>

#define DEBUG_DEPENDS_TRACKER 0

using namespace std;

namespace ixion {

namespace {

class cell_printer : public unary_function<const formula_cell*, void>
{
public:
    cell_printer(const iface::model_context& cxt) : m_cxt(cxt) {}

    void operator() (const formula_cell* p) const
    {
        __IXION_DEBUG_OUT__ << "  " << m_cxt.get_cell_name(p) << endl;
    }

private:
    const iface::model_context& m_cxt;
};

/**
 * Function object to reset the status of formula cell to pre-interpretation
 * status.
 */
struct cell_reset_handler : public unary_function<formula_cell*, void>
{
    void operator() (formula_cell* p) const
    {
        p->reset();
    }
};

class circular_check_handler : public unary_function<formula_cell*, void>
{
    const iface::model_context& m_context;
public:
    circular_check_handler(const iface::model_context& cxt) : m_context(cxt) {}

    void operator() (formula_cell* p) const
    {
        p->check_circular(m_context);
    }
};

struct thread_queue_handler : public unary_function<formula_cell*, void>
{
    void operator() (formula_cell* p) const
    {
        cell_queue_manager::add_cell(p);
    }
};

struct cell_interpret_handler : public unary_function<base_cell*, void>
{
    cell_interpret_handler(iface::model_context& cxt) :
        m_context(cxt) {}

    void operator() (base_cell* p) const
    {
        assert(p->get_celltype() == celltype_formula);
        static_cast<formula_cell*>(p)->interpret(m_context);
    }
private:
    iface::model_context& m_context;
};

}

dependency_tracker::cell_back_inserter::cell_back_inserter(vector<formula_cell*> & sorted_cells) :
    m_sorted_cells(sorted_cells) {}

void dependency_tracker::cell_back_inserter::operator() (formula_cell* cell)
{
    m_sorted_cells.push_back(cell);
}

// ============================================================================

dependency_tracker::dependency_tracker(
    const dirty_cells_t& dirty_cells, iface::model_context& cxt) :
    m_dirty_cells(dirty_cells), m_context(cxt)
{
}

dependency_tracker::~dependency_tracker()
{
}

void dependency_tracker::insert_depend(formula_cell* origin_cell, formula_cell* depend_cell)
{
#if DEBUG_DEPENDS_TRACKER
    __IXION_DEBUG_OUT__ << m_context.get_cell_name(origin_cell) << "->" << m_context.get_cell_name(depend_cell) << endl;
#endif
    m_deps.insert(origin_cell, depend_cell);
}

void dependency_tracker::interpret_all_cells(size_t thread_count)
{
    vector<formula_cell*> sorted_cells;
    topo_sort_cells(sorted_cells);

#if DEBUG_DEPENDS_TRACKER
    __IXION_DEBUG_OUT__ << "Topologically sorted cells ---------------------------------" << endl;
    for_each(sorted_cells.begin(), sorted_cells.end(), cell_printer(m_context));
#endif

    // Reset cell status.
#if DEBUG_DEPENDS_TRACKER
    __IXION_DEBUG_OUT__ << "Reset cell status ------------------------------------------" << endl;
#endif
    for_each(sorted_cells.begin(), sorted_cells.end(), cell_reset_handler());

    // First, detect circular dependencies and mark those circular
    // dependent cells with appropriate error flags.
#if DEBUG_DEPENDS_TRACKER
    __IXION_DEBUG_OUT__ << "Check circular dependencies --------------------------------" << endl;
#endif
    for_each(sorted_cells.begin(), sorted_cells.end(), circular_check_handler(m_context));

    if (thread_count > 0)
    {
        // Interpret cells in topological order using threads.
        cell_queue_manager::init(thread_count, m_context);
        for_each(sorted_cells.begin(), sorted_cells.end(), thread_queue_handler());
        cell_queue_manager::terminate();
    }
    else
    {
        // Interpret cells using just a single thread.
        for_each(sorted_cells.begin(), sorted_cells.end(), cell_interpret_handler(m_context));
    }
}

void dependency_tracker::topo_sort_cells(vector<formula_cell*>& sorted_cells) const
{
    cell_back_inserter handler(sorted_cells);
    vector<formula_cell*> all_cells;
    all_cells.reserve(m_dirty_cells.size());
    dirty_cells_t::const_iterator itr = m_dirty_cells.begin(), itr_end = m_dirty_cells.end();
    for (; itr != itr_end; ++itr)
        all_cells.push_back(*itr);

    dfs_type dfs(all_cells, m_deps.get(), handler);
    dfs.run();
}

}
