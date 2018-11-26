/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ixion/model_iterator.hpp"
#include "ixion/global.hpp"
#include "ixion/exceptions.hpp"
#include "ixion/model_context.hpp"

#include <mdds/multi_type_vector/collection.hpp>
#include <sstream>
#include <ostream>

namespace ixion {

model_iterator::cell::cell() : row(0), col(0), type(celltype_t::empty) {}

model_iterator::cell::cell(row_t _row, col_t _col) :
    row(_row), col(_col), type(celltype_t::empty) {}

model_iterator::cell::cell(row_t _row, col_t _col, bool _b) :
    row(_row), col(_col), type(celltype_t::boolean)
{
    value.boolean = _b;
}

model_iterator::cell::cell(row_t _row, col_t _col, string_id_t _s) :
    row(_row), col(_col), type(celltype_t::string)
{
    value.string = _s;
}

model_iterator::cell::cell(row_t _row, col_t _col, double _v) :
    row(_row), col(_col), type(celltype_t::numeric)
{
    value.numeric = _v;
}

model_iterator::cell::cell(row_t _row, col_t _col, const formula_cell* _f) :
    row(_row), col(_col), type(celltype_t::formula)
{
    value.formula = _f;
}

bool model_iterator::cell::operator== (const cell& other) const
{
    if (type != other.type || row != other.row || col != other.col)
        return false;

    switch (type)
    {
        case celltype_t::empty:
            return true;
        case celltype_t::numeric:
            return value.numeric == other.value.numeric;
        case celltype_t::boolean:
            return value.boolean == other.value.boolean;
        case celltype_t::string:
            return value.string == other.value.string;
        case celltype_t::formula:
            // Compare by the formula cell memory address.
            return value.formula == other.value.formula;
        default:
            ;
    }

    return false;
}

class model_iterator::impl
{
public:
    virtual bool has() const = 0;
    virtual void next() = 0;
    virtual const model_iterator::cell& get() const = 0;
};

namespace {

class iterator_core_empty : public model_iterator::impl
{
    model_iterator::cell m_cell;
public:
    bool has() const override { return false; }

    void next() override {}

    const model_iterator::cell& get() const override
    {
        return m_cell;
    }
};


class iterator_core_horizontal : public model_iterator::impl
{
    using collection_type = mdds::mtv::collection<column_store_t>;

    collection_type m_collection;
    mutable model_iterator::cell m_current_cell;
    mutable bool m_update_current_cell;
    collection_type::const_iterator m_current_pos;
    collection_type::const_iterator m_end;

    void update_current() const
    {
        m_current_cell.col = m_current_pos->index;
        m_current_cell.row = m_current_pos->position;

        switch (m_current_pos->type)
        {
            case element_type_boolean:
                m_current_cell.type = celltype_t::boolean;
                m_current_cell.value.boolean = m_current_pos->get<boolean_element_block>();
                break;
            case element_type_numeric:
                m_current_cell.type = celltype_t::numeric;
                m_current_cell.value.numeric = m_current_pos->get<numeric_element_block>();
                break;
            case element_type_string:
                m_current_cell.type = celltype_t::string;
                m_current_cell.value.string = m_current_pos->get<string_element_block>();
                break;
            case element_type_formula:
                m_current_cell.type = celltype_t::formula;
                m_current_cell.value.formula = m_current_pos->get<formula_element_block>();
                break;
            case element_type_empty:
                m_current_cell.type = celltype_t::empty;
            default:
                ;
        }

        m_update_current_cell = false;
    }
public:
    iterator_core_horizontal(const model_context& cxt, sheet_t sheet) :
        m_update_current_cell(true)
    {
        const column_stores_t* cols = cxt.get_columns(sheet);
        if (cols)
        {
            collection_type c = mdds::mtv::collection<column_store_t>(cols->begin(), cols->end());
            m_collection.swap(c);
        }

        m_current_pos = m_collection.begin();
        m_end = m_collection.end();
    }

    virtual bool has() const override
    {
        return m_current_pos != m_end;
    }

    virtual void next() override
    {
        ++m_current_pos;
        m_update_current_cell = true;
    }

    virtual const model_iterator::cell& get() const override
    {
        if (m_update_current_cell)
            update_current();
        return m_current_cell;
    }
};

class iterator_core_vertical : public model_iterator::impl
{
    const column_stores_t* m_cols;
    mutable model_iterator::cell m_current_cell;
    mutable bool m_update_current_cell;

    column_stores_t::const_iterator m_it_cols;
    column_stores_t::const_iterator m_it_cols_begin;
    column_stores_t::const_iterator m_it_cols_end;

    column_store_t::const_position_type m_current_pos;
    column_store_t::const_position_type m_end_pos;

    void update_current() const
    {
        column_store_t::const_iterator blk_pos = m_current_pos.first;

        switch (blk_pos->type)
        {
            case element_type_empty:
                m_current_cell.type = celltype_t::empty;
                break;
            case element_type_boolean:
                m_current_cell.type = celltype_t::boolean;
                m_current_cell.value.boolean = column_store_t::get<boolean_element_block>(m_current_pos);
                break;
            case element_type_numeric:
                m_current_cell.type = celltype_t::numeric;
                m_current_cell.value.numeric = column_store_t::get<numeric_element_block>(m_current_pos);
                break;
            case element_type_string:
                m_current_cell.type = celltype_t::string;
                m_current_cell.value.string = column_store_t::get<string_element_block>(m_current_pos);
                break;
            case element_type_formula:
                m_current_cell.type = celltype_t::formula;
                m_current_cell.value.formula = column_store_t::get<formula_element_block>(m_current_pos);
                break;
            default:
                throw std::logic_error("unhandled element type.");
        }

        m_current_cell.row = column_store_t::logical_position(m_current_pos);
        m_current_cell.col = std::distance(m_it_cols_begin, m_it_cols);
        m_update_current_cell = false;
    }

public:
    iterator_core_vertical(const model_context& cxt, sheet_t sheet) :
        m_update_current_cell(true)
    {
        m_cols = cxt.get_columns(sheet);
        if (!m_cols)
            return;

        m_it_cols_begin = m_cols->begin();
        m_it_cols = m_it_cols_begin;
        m_it_cols_end = m_cols->end();
        if (m_it_cols_begin == m_it_cols_end)
            return;

        const column_store_t& col = **m_it_cols;
        m_current_pos = col.position(0);
    }

    bool has() const override
    {
        if (!m_cols)
            return false;

        return m_it_cols != m_it_cols_end;
    }

    void next() override
    {
        m_update_current_cell = true;
        m_current_pos = column_store_t::advance_position(m_current_pos, 1);

        const column_store_t* col = *m_it_cols;
        if (m_current_pos.first != col->cend())
            // It hasn't reached the end of the current column yet.
            return;

        ++m_it_cols; // Move to the next column.
        if (m_it_cols == m_it_cols_end)
            return;

        // Reset the position to the first cell in the new column.
        col = *m_it_cols;
        m_current_pos = col->position(0);
    }

    const model_iterator::cell& get() const override
    {
        if (m_update_current_cell)
            update_current();
        return m_current_cell;
    }
};

} // anonymous namespace

model_iterator::model_iterator() : mp_impl(ixion::make_unique<iterator_core_empty>()) {}
model_iterator::model_iterator(const model_context& cxt, sheet_t sheet, rc_direction_t dir)
{
    switch (dir)
    {
        case rc_direction_t::horizontal:
            mp_impl = ixion::make_unique<iterator_core_horizontal>(cxt, sheet);
            break;
        case rc_direction_t::vertical:
            mp_impl = ixion::make_unique<iterator_core_vertical>(cxt, sheet);
            break;
    }
}

model_iterator::model_iterator(model_iterator&& other) : mp_impl(std::move(other.mp_impl)) {}

model_iterator::~model_iterator() {}

model_iterator& model_iterator::operator= (model_iterator&& other)
{
    mp_impl = std::move(other.mp_impl);
    other.mp_impl = ixion::make_unique<iterator_core_empty>();
    return *this;
}

bool model_iterator::has() const
{
    return mp_impl->has();
}

void model_iterator::next()
{
    mp_impl->next();
}

const model_iterator::cell& model_iterator::get() const
{
    return mp_impl->get();
}

std::ostream& operator<< (std::ostream& os, const model_iterator::cell& c)
{
    os << "(row=" << c.row << "; col=" << c.col << "; type=" << short(c.type);

    switch (c.type)
    {
        case celltype_t::boolean:
            os << "; boolean=" << c.value.boolean;
            break;
        case celltype_t::formula:
            os << "; formula=" << c.value.formula;
            break;
        case celltype_t::numeric:
            os << "; numeric=" << c.value.numeric;
            break;
        case celltype_t::string:
            os << "; string=" << c.value.string;
            break;
        case celltype_t::empty:
        case celltype_t::unknown:
        default:
            ;
    }

    os << ')';
    return os;
}

} // namespace ixion

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */