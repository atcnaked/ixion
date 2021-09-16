/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ixion/formula_name_resolver.hpp"
#include "ixion/interface/formula_model_access.hpp"
#include "ixion/mem_str_buf.hpp"
#include "ixion/table.hpp"

#include "formula_functions.hpp"
#include "debug.hpp"

#include <cassert>
#include <iostream>
#include <sstream>
#include <vector>
#include <limits>
#include <algorithm>

using namespace std;

namespace ixion {

namespace {

bool check_address_by_sheet_bounds(const iface::formula_model_access* cxt, const address_t& pos)
{
    rc_size_t ss(row_upper_bound, column_upper_bound);

    if (cxt && pos.sheet >= 0 && size_t(pos.sheet) < cxt->get_sheet_count())
    {
        // Make sure the address is within the sheet size.
        ss = cxt->get_sheet_size();
    }

    row_t row_check = pos.row >= 0 ? pos.row : -pos.row;

    if (pos.row != row_unset && row_check >= ss.row)
        return false;

    col_t col_check = pos.column >= 0 ? pos.column : -pos.column;

    if (pos.column != column_unset && col_check >= ss.column)
        return false;

    return true;
}

bool resolve_function(const char* p, size_t n, formula_name_t& ret)
{
    formula_function_t func_oc = formula_functions::get_function_opcode({p, n});
    if (func_oc != formula_function_t::func_unknown)
    {
        // This is a built-in function.
        ret.type = formula_name_t::function;
        ret.func_oc = func_oc;
        return true;
    }
    return false;
}

/**
 * Table reference can be either one of:
 *
 * <ul>
 * <li>Table[Column]</li>
 * <li>[Column]</li>
 * <li>Table[[#Area],[Column]]</li>
 * <li>Table[[#Area1],[#Area2],[Column]]</li>
 * </ul>
 *
 * where the #Area (area specifier) can be one or more of
 *
 * <ul>
 * <li>#Header</li>
 * <li>#Data</li>
 * <li>#Totals</li>
 * <li>#All</li>
 * </ul>
 */
bool resolve_table(const iface::formula_model_access* cxt, const char* p, size_t n, formula_name_t& ret)
{
    if (!cxt)
        return false;

    short scope = 0;
    size_t last_column_pos = std::numeric_limits<size_t>::max();
    mem_str_buf buf;
    mem_str_buf table_name;
    vector<mem_str_buf> names;

    bool table_detected = false;

    const char* p_end = p + n;
    for (; p != p_end; ++p)
    {
        switch (*p)
        {
            case '[':
            {
                if (scope >= 2)
                    return false;

                table_detected = true;
                if (!buf.empty())
                {
                    if (scope != 0)
                        return false;

                    table_name = buf;
                    buf.clear();
                }

                ++scope;
            }
            break;
            case ']':
            {
                if (scope <= 0)
                    // non-matching brace.
                    return false;

                if (!buf.empty())
                {
                    names.push_back(buf);
                    buf.clear();
                }

                --scope;
            }
            break;
            case ',':
            {
                if (!buf.empty())
                    return false;
            }
            break;
            case ':':
            {
                if (scope != 1)
                    // allowed only inside the first scope.
                    return false;

                if (!buf.empty())
                    return false;

                if (names.empty())
                    return false;

                last_column_pos = names.size();
            }
            break;
            default:
                if (buf.empty())
                    buf.set_start(p);
                else
                    buf.inc();
        }
    }

    if (!buf.empty())
        return false;

    if (!table_detected)
        return false;

    if (names.empty())
        return false;

    ret.table.areas = table_area_none;
    ret.type = formula_name_t::table_reference;
    ret.table.name = table_name.get();
    ret.table.name_length = table_name.size();
    ret.table.column_first = NULL;
    ret.table.column_first_length = 0;
    ret.table.column_last = NULL;
    ret.table.column_last_length = 0;

    vector<mem_str_buf>::iterator it = names.begin(), it_end = names.end();
    for (; it != it_end; ++it)
    {
        buf = *it;
        assert(!buf.empty());
        if (buf[0] == '#')
        {
            // area specifier.
            buf.pop_front();
            if (buf.equals("Headers"))
                ret.table.areas |= table_area_headers;
            else if (buf.equals("Data"))
                ret.table.areas |= table_area_data;
            else if (buf.equals("Totals"))
                ret.table.areas |= table_area_totals;
            else if (buf.equals("All"))
                ret.table.areas = table_area_all;
        }
        else if (ret.table.column_first_length)
        {
            // This is a second column name.
            if (ret.table.column_last_length)
                return false;

            size_t dist = std::distance(names.begin(), it);
            if (dist != last_column_pos)
                return false;

            ret.table.column_last = buf.get();
            ret.table.column_last_length = buf.size();
        }
        else
        {
            // first column name.
            if (!ret.table.areas)
                ret.table.areas = table_area_data;

            ret.table.column_first = buf.get();
            ret.table.column_first_length = buf.size();
        }
    }

    return true;
}

/**
 * Check if the name is a built-in function, or else it's considered a named
 * expression.
 *
 * @param name name to be resolved
 * @param ret resolved name type
 */
void resolve_function_or_name(const char* p, size_t n, formula_name_t& ret)
{
    if (resolve_function(p, n, ret))
        return;

    // Everything else is assumed to be a named expression.
    ret.type = formula_name_t::named_expression;
}

void set_address(formula_name_t::address_type& dest, const address_t& addr)
{
    dest.sheet = addr.sheet;
    dest.row = addr.row;
    dest.col = addr.column;
    dest.abs_sheet = addr.abs_sheet;
    dest.abs_row = addr.abs_row;
    dest.abs_col = addr.abs_column;
}

void set_cell_reference(formula_name_t& ret, const address_t& addr)
{
    ret.type = formula_name_t::cell_reference;
    set_address(ret.address, addr);
}

enum class resolver_parse_mode { column, row };

void append_sheet_name(ostringstream& os, const ixion::iface::formula_model_access& cxt, sheet_t sheet)
{
    if (!is_valid_sheet(sheet))
    {
        IXION_DEBUG("invalid sheet index (" << sheet << ")");
        return;
    }

    string sheet_name = cxt.get_sheet_name(sheet);
    string buffer; // used only when the sheet name contains at least one single quote.

    const char* p = sheet_name.data();
    const char* p_end = p + sheet_name.size();

    bool quote = false;
    const char* p0 = nullptr;

    for (; p != p_end; ++p)
    {
        if (!p0)
            p0 = p;

        switch (*p)
        {
            case ' ':
                quote = true;
            break;
            case '\'':
                quote = true;
                buffer += string(p0, p-p0);
                buffer.push_back(*p);
                buffer.push_back(*p);
                p0 = nullptr;
            break;
        }
    }

    if (quote)
        os << '\'';

    if (buffer.empty())
        os << sheet_name;
    else
    {
        if (p0)
            buffer += string(p0, p-p0);
        os << buffer;
    }

    if (quote)
        os << '\'';
}

void append_sheet_name_calc_a1(
    std::ostringstream& os, const ixion::iface::formula_model_access* cxt, const address_t& addr, const abs_address_t& origin)
{
    if (!cxt)
        return;

    sheet_t sheet = addr.sheet;
    if (addr.abs_sheet)
        os << '$';
    else
        sheet += origin.sheet;
    append_sheet_name(os, *cxt, sheet);
    os << '.';
}

void append_sheet_name_odf_cra(
    std::ostringstream& os, const ixion::iface::formula_model_access* cxt, const address_t& addr, const abs_address_t& origin)
{
    if (cxt)
    {
        sheet_t sheet = addr.sheet;
        if (addr.abs_sheet)
            os << '$';
        else
            sheet += origin.sheet;
        append_sheet_name(os, *cxt, sheet);
    }
    os << '.';
}

void append_column_name_a1(ostringstream& os, col_t col)
{
    const col_t div = 26;
    string col_name;
    while (true)
    {
        col_t rem = col % div;
        char c = 'A' + rem;
        col_name.push_back(c);
        if (col < div)
            break;

        col -= rem;
        col /= div;
        col -= 1;
    }

    std::reverse(col_name.begin(), col_name.end());
    os << col_name;
}

void append_column_address_a1(std::ostringstream& os, col_t col, col_t origin, bool absolute)
{
    if (col == column_unset)
        return;

    if (absolute)
        os << '$';
    else
        col += origin;

    append_column_name_a1(os, col);
}

void append_row_address_a1(std::ostringstream& os, row_t row, row_t origin, bool absolute)
{
    if (row == row_unset)
        return;

    if (absolute)
        os << '$';
    else
        row += origin;

    os << (row + 1);
}

void append_address_a1(
    std::ostringstream& os, const ixion::iface::formula_model_access* cxt,
    const address_t& addr, const abs_address_t& pos, char sheet_name_sep)
{
    assert(sheet_name_sep);

    col_t col = addr.column;
    row_t row = addr.row;
    sheet_t sheet = addr.sheet;
    if (!addr.abs_column)
        col += pos.column;
    if (!addr.abs_row)
        row += pos.row;
    if (!addr.abs_sheet)
        sheet += pos.sheet;

    if (cxt)
    {
        append_sheet_name(os, *cxt, sheet);
        os << sheet_name_sep;
    }

    if (addr.abs_column)
        os << '$';
    append_column_name_a1(os, col);

    if (addr.abs_row)
        os << '$';
    os << (row + 1);
}

/**
 * Almost identical to append_address_a1, but it always appends a sheet name
 * separator even if a sheet name is not appended.
 */
void append_address_a1_with_sheet_name_sep(
    std::ostringstream& os, const ixion::iface::formula_model_access* cxt,
    const address_t& addr, const abs_address_t& pos, char sheet_name_sep)
{
    if (!cxt)
        os << sheet_name_sep;

    append_address_a1(os, cxt, addr, pos, sheet_name_sep);
}

void append_address_r1c1(
    ostringstream& os, const address_t& addr, const abs_address_t& pos)
{
    if (addr.row != row_unset)
    {
        os << 'R';
        if (addr.abs_row)
            // absolute row address.
            os << (addr.row+1);
        else if (addr.row)
        {
            // relative row address different from origin.
            os << '[';
            os << addr.row;
            os << ']';
        }

    }
    if (addr.column != column_unset)
    {
        os << 'C';
        if (addr.abs_column)
            os << (addr.column+1);
        else if (addr.column)
        {
            os << '[';
            os << addr.column;
            os << ']';
        }
    }
}

void append_name_string(ostringstream& os, const iface::formula_model_access* cxt, string_id_t sid)
{
    if (!cxt)
        return;

    const string* p = cxt->get_string(sid);
    if (p)
        os << *p;
}

char append_table_areas(ostringstream& os, const table_t& table)
{
    if (table.areas == table_area_all)
    {
        os << "[#All]";
        return 1;
    }

    bool headers = (table.areas & table_area_headers);
    bool data = (table.areas & table_area_data);
    bool totals = (table.areas & table_area_totals);

    char count = 0;
    if (headers)
    {
        os << "[#Headers]";
        ++count;
    }

    if (data)
    {
        if (count > 0)
            os << ',';
        os << "[#Data]";
        ++count;
    }

    if (totals)
    {
        if (count > 0)
            os << ',';
        os << "[#Totals]";
        ++count;
    }

    return count;
}

enum parse_address_result_type
{
    invalid = 0,
    valid_address,
    range_expected /// valid address followed by a ':'.
};

struct parse_address_result
{
    parse_address_result_type result;
    bool sheet_name = false;
};

#if IXION_LOGGING

std::ostream& operator<< (std::ostream& os, parse_address_result_type rt)
{
    static const char* names[] = {
        "invalid",
        "valid address",
        "range expected"
    };

    os << names[rt];
    return os;
}

std::string to_string(parse_address_result_type rt)
{
    std::ostringstream os;
    os << rt;
    return os.str();
}

#endif

bool parse_sheet_name_quoted(
    const ixion::iface::formula_model_access& cxt, const char sep, const char*& p, const char* p_last, sheet_t& sheet)
{
    ++p; // skip the open quote.
    size_t len = 0;
    string buffer; // used only when the name contains at least one single quote.
    const char* p1 = p;

    // parse until the closing quote is reached.
    while (true)
    {
        if (*p == '\'')
        {
            if (p == p_last)
                break;

            if (*(p+1) == '\'')
            {
                // next character is a quote too.  Store the parsed string
                // segment to the buffer and move on.
                ++p;
                ++len;
                buffer += string(p1, len);
                ++p;
                p1 = p;
                len = 0;
                continue;
            }

            if (*(p+1) != sep)
                // the next char must be the separator.  Parse failed.
                break;

            if (buffer.empty())
                // Name contains no single quotes.
                sheet = cxt.get_sheet_index(p1, len);
            else
            {
                buffer += string(p1, len);
                sheet = cxt.get_sheet_index(buffer.data(), buffer.size());
            }

            ++p; // skip the closing quote.
            if (p != p_last)
                ++p; // skip the separator.
            return true;
        }

        if (p == p_last)
            break;

        ++p;
        ++len;
    }

    return false;
}

/**
 * Try to parse a sheet name prefix in the string.  If this fails, revert
 * the current position back to the original position prior to the call.
 *
 * @return true if the string contains a valid sheet name, false otherwise.
 */
bool parse_sheet_name(
    const ixion::iface::formula_model_access& cxt, const char sep, const char*& p, const char* p_last, sheet_t& sheet)
{
    assert(p <= p_last);

    const char* p_old = p; // old position to revert to in case we fail to parse a sheet name.

    if (*p == '$')
        ++p;

    if (*p == '\'')
    {
        bool success = parse_sheet_name_quoted(cxt, sep, p, p_last, sheet);
        if (!success)
            p = p_old;
        return success;
    }

    const char* p0 = p;
    size_t len = 0;

    // parse until we hit the sheet-address separator.
    while (true)
    {
        if (*p == sep)
        {
            sheet = cxt.get_sheet_index(p0, len);
            if (p != p_last)
                ++p; // skip the separator.
            return true;
        }

        if (p == p_last)
            break;

        ++p;
        ++len;
    }

    p = p_old;
    return false;
}

/**
 * If there is no number to parse, it returns 0 and the p will not
 * increment. Otherwise, p will point to the last digit of the number when
 * the call returns.
 */
template<typename T>
T parse_number(const char*&p, const char* p_last)
{
    T num = 0;

    bool sign = false;
    if (*p == '+')
        ++p;
    else if (*p == '-')
    {
        ++p;
        sign = true;
    }

    bool all_digits = false;
    while (std::isdigit(*p))
    {
        // Parse number.
        num *= 10;
        num += *p - '0';
        if (p == p_last)
        {
            all_digits = true;
            break;
        }
        ++p;
    }

    if (!all_digits)
        --p;

    if (sign)
        num *= -1;

    return num;
}

/**
 * Parse A1-style single cell address.
 *
 * @param p it must point to the first character of a cell address.
 * @param p_last it must point to the last character of a whole string
 *               sequence.  It doesn't have to be the last character of a
 *               cell address.
 * @param addr resolved cell address.
 *
 * @return parsing result.
 */
parse_address_result_type parse_address_a1(const char*& p, const char* p_last, address_t& addr)
{
    // NOTE: Row and column IDs are 1-based during parsing, while 0 is used as
    // the state of a value-not-set.  They are subtracted by one before
    // returning.

    resolver_parse_mode mode = resolver_parse_mode::column;

    while (true)
    {
        char c = *p;
        if ('a' <= c && c <= 'z')
        {
            // Convert to upper case.
            c -= 'a' - 'A';
        }

        if ('A' <= c && c <= 'Z')
        {
            // Column digit
            if (mode != resolver_parse_mode::column)
                return invalid;

            if (addr.column)
                addr.column *= 26;
            addr.column += static_cast<col_t>(c - 'A' + 1);

            if (addr.column > column_upper_bound)
                return invalid;
        }
        else if (std::isdigit(c))
        {
            if (mode == resolver_parse_mode::column)
            {
                // First digit of a row.
                if (c == '0')
                    // Leading zeros not allowed.
                    return invalid;

                mode = resolver_parse_mode::row;
            }

            if (addr.row)
                addr.row *= 10;

            addr.row += static_cast<row_t>(c - '0');
        }
        else if (c == ':')
        {
            if (mode == resolver_parse_mode::row)
            {
                if (!addr.row)
                    return invalid;

                --addr.row;

                if (addr.column)
                    --addr.column;
                else
                    addr.column = column_unset;

                return range_expected;
            }
            else if (mode == resolver_parse_mode::column)
            {
                // row number is not given.
                if (addr.column)
                    --addr.column;
                else
                    // neither row number nor column number is given.
                    return invalid;

                addr.row = row_unset;
                return range_expected;
            }
            else
                return invalid;
        }
        else if (c == '$')
        {
            // Absolute position.
            if (mode == resolver_parse_mode::column)
            {
                if (addr.column)
                {
                    // Column position has been already parsed.
                    mode = resolver_parse_mode::row;
                    addr.abs_row = true;
                }
                else
                {
                    // Column position has not yet been parsed.
                    addr.abs_column = true;
                }
            }
            else
                return invalid;
        }
        else
            return invalid;

        if (p == p_last)
            // last character reached.
            break;
        ++p;
    }

    if (mode == resolver_parse_mode::row)
    {
        if (!addr.row)
            return invalid;

        --addr.row;

        if (addr.column)
            --addr.column;
        else
            addr.column = column_unset;
    }
    else if (mode == resolver_parse_mode::column)
    {
        // row number is not given.
        if (addr.column)
            --addr.column;
        else
            // neither row number nor column number is given.
            return invalid;

        addr.row = row_unset;
    }
    return valid_address;
}

parse_address_result_type parse_address_r1c1(const char*& p, const char* p_last, address_t& addr)
{
    addr.row = row_unset;
    addr.column = column_unset;

    if (*p == 'R' || *p == 'r')
    {
        addr.row = 0;
        addr.abs_row = false;

        if (p == p_last)
            // Just 'R'.  Not sure if this is valid or invalid, but let's call it invalid for now.
            return parse_address_result_type::invalid;

        ++p;
        if (*p != 'C' && *p != 'c')
        {
            addr.abs_row = (*p != '[');
            if (!addr.abs_row)
            {
                // Relative row address.
                ++p;
                if (!std::isdigit(*p) && *p != '-' && *p != '+')
                    return parse_address_result_type::invalid;

                addr.row = parse_number<row_t>(p, p_last);
                ++p;
                if (p == p_last)
                    return (*p == ']') ? parse_address_result_type::valid_address : parse_address_result_type::invalid;
                ++p;
            }
            else if (std::isdigit(*p))
            {
                // Absolute row address.
                addr.row = parse_number<row_t>(p, p_last);
                if (addr.row <= 0)
                    // absolute address with 0 or negative value is invalid.
                    return parse_address_result_type::invalid;

                --addr.row; // 1-based to 0-based.

                if (p == p_last && std::isdigit(*p))
                    // 'R' followed by a number without 'C' is valid.
                    return parse_address_result_type::valid_address;
                ++p;
            }
        }
    }

    if (*p == 'C' || *p == 'c')
    {
        addr.column = 0;
        addr.abs_column = false;

        if (p == p_last)
        {
            if (addr.row == row_unset)
                // Just 'C'.  Row must be set.
                return parse_address_result_type::invalid;

            if (!addr.abs_row && addr.row == 0)
                // 'RC' is invalid as it references itself.
                return parse_address_result_type::invalid;

            return parse_address_result_type::valid_address;
        }

        ++p;
        if (*p == '[')
        {
            // Relative column address.
            ++p;
            if (!std::isdigit(*p) && *p != '-' && *p != '+')
                return parse_address_result_type::invalid;

            addr.column = parse_number<col_t>(p, p_last);
            ++p;
            if (p == p_last)
                return (*p == ']') ? parse_address_result_type::valid_address : parse_address_result_type::invalid;

            ++p;
        }
        else if (std::isdigit(*p))
        {
            // Absolute column address.
            addr.abs_column = true;
            addr.column = parse_number<col_t>(p, p_last);
            if (addr.column <= 0)
                // absolute address with 0 or negative value is invalid.
                return parse_address_result_type::invalid;

            --addr.column; // 1-based to 0-based.

            if (p == p_last)
                return parse_address_result_type::valid_address;

            ++p;
        }
    }

    if (*p == ':')
        return (p == p_last) ? parse_address_result_type::invalid : parse_address_result_type::range_expected;

    return parse_address_result_type::invalid;
}

parse_address_result parse_address_calc_a1(
    const ixion::iface::formula_model_access* cxt, const char*& p, const char* p_last, address_t& addr)
{
    parse_address_result res;

    addr.row = 0;
    addr.column = 0;
    addr.abs_row = false;
    addr.abs_column = false;

    if (cxt)
    {
        // Overwrite the sheet index *only when* the sheet name is parsed successfully.
        const char* p0 = p;
        res.sheet_name = parse_sheet_name(*cxt, '.', p, p_last, addr.sheet);
        if (res.sheet_name)
            addr.abs_sheet = (*p0 == '$');
    }

    res.result = parse_address_a1(p, p_last, addr);
    return res;
}

/**
 * Parse a single cell address in ODF cell-range-address syntax.  This is
 * almost identical to Calc A1 except that it allows a leading '.' as in
 * '.E1' as opposed to just 'E1'.
 */
parse_address_result parse_address_odf_cra(
    const ixion::iface::formula_model_access* cxt, const char*& p, const char* p_last, address_t& addr)
{
    if (*p == '.')
    {
        // Skip the '.', and assume absence of sheet name.
        ++p;
        cxt = nullptr;
    }

    return parse_address_calc_a1(cxt, p, p_last, addr);
}

parse_address_result_type parse_address_excel_a1(
    const ixion::iface::formula_model_access* cxt, const char*& p, const char* p_last, address_t& addr)
{
    addr.row = 0;
    addr.column = 0;
    addr.abs_sheet = true; // Excel's sheet position is always absolute.
    addr.abs_row = false;
    addr.abs_column = false;

    if (cxt)
        // Overwrite the sheet index *only when* the sheet name is parsed successfully.
        parse_sheet_name(*cxt, '!', p, p_last, addr.sheet);

    return parse_address_a1(p, p_last, addr);
}

parse_address_result_type parse_address_excel_r1c1(
    const iface::formula_model_access* cxt, const char*& p, const char* p_last, address_t& addr)
{
    addr.row = 0;
    addr.column = 0;
    addr.abs_sheet = true; // Excel's sheet position is always absolute.
    addr.abs_row = false;
    addr.abs_column = false;

    if (cxt)
        // Overwrite the sheet index *only when* the sheet name is parsed successfully.
        parse_sheet_name(*cxt, '!', p, p_last, addr.sheet);

    return parse_address_r1c1(p, p_last, addr);
}

parse_address_result parse_address_odff(
    const ixion::iface::formula_model_access* cxt, const char*& p, const char* p_last, address_t& addr)
{
    parse_address_result res;
    assert(p <= p_last);

    addr.row = 0;
    addr.column = 0;
    addr.abs_row = false;
    addr.abs_column = false;

    if (*p == '.')
    {
        // This address doesn't contain a sheet name.
        ++p;
    }
    else if (cxt)
    {
        // This address DOES contain a sheet name.
        res.sheet_name = true;
        addr.abs_sheet = false;
        addr.sheet = invalid_sheet;

        // Overwrite the sheet index *only when* the sheet name is parsed successfully.
        if (*p == '$')
        {
            addr.abs_sheet = true;
            ++p;
        }

        if (p <= p_last)
            parse_sheet_name(*cxt, '.', p, p_last, addr.sheet);
    }

    res.result = parse_address_a1(p, p_last, addr);
    return res;
}

string abs_or_rel(bool _abs)
{
    return _abs ? "(abs)" : "(rel)";
}

string _to_string(const formula_name_t::address_type& addr)
{
    std::ostringstream os;
    os << "[sheet=" << addr.sheet << abs_or_rel(addr.abs_sheet) << ",row="
        << addr.row << abs_or_rel(addr.abs_row) << ",column="
        << addr.col << abs_or_rel(addr.abs_col) << "]";

    return os.str();
}

void to_relative_address(address_t& addr, const abs_address_t& pos, bool sheet)
{
    if (!addr.abs_sheet && sheet)
        addr.sheet -= pos.sheet;
    if (!addr.abs_row && addr.row <= row_upper_bound)
        addr.row -= pos.row;
    if (!addr.abs_column && addr.column <= column_upper_bound)
        addr.column -= pos.column;
}

string to_string(const iface::formula_model_access* cxt, const table_t& table)
{
    ostringstream os;
    append_name_string(os, cxt, table.name);

    if (table.column_first == empty_string_id)
    {
        // Area specifier(s) only.
        bool headers = (table.areas & table_area_headers);
        bool data = (table.areas & table_area_data);
        bool totals = (table.areas & table_area_totals);

        short count = 0;
        if (headers)
            ++count;
        if (data)
            ++count;
        if (totals)
            ++count;

        bool multiple = count == 2;
        if (multiple)
            os << '[';

        append_table_areas(os, table);

        if (multiple)
            os << ']';
    }
    else if (table.column_last == empty_string_id)
    {
        // single column.
        os << '[';

        bool multiple = false;
        if (table.areas && table.areas != table_area_data)
        {
            if (append_table_areas(os, table))
            {
                os << ',';
                multiple = true;
            }
        }

        if (multiple)
            os << '[';

        append_name_string(os, cxt, table.column_first);

        if (multiple)
            os << ']';

        os << ']';
    }
    else
    {
        // column range.
        os << '[';

        if (table.areas && table.areas != table_area_data)
        {
            if (append_table_areas(os, table))
                os << ',';
        }

        os << '[';
        append_name_string(os, cxt, table.column_first);
        os << "]:[";
        append_name_string(os, cxt, table.column_last);
        os << "]]";
    }

    return os.str();
}

} // anonymous namespace

formula_name_t::formula_name_t() : type(invalid) {}

string formula_name_t::to_string() const
{
    std::ostringstream os;

    switch (type)
    {
        case cell_reference:
            os << "cell reference: " << _to_string(address);
            break;
        case function:
            os << "function";
            break;
        case invalid:
            os << "invalid";
            break;
        case named_expression:
            os << "named expression";
            break;
        case range_reference:
            os << "range raference: first: " << _to_string(range.first) << "  last: "
                << _to_string(range.last) << endl;
            break;
        case table_reference:
            os << "table reference";
            break;
        default:
            os << "unknown foromula name type";
    }

    return os.str();
}

address_t to_address(const formula_name_t::address_type& src)
{
    address_t addr;

    addr.sheet      = src.sheet;
    addr.abs_sheet  = src.abs_sheet;
    addr.row        = src.row;
    addr.abs_row    = src.abs_row;
    addr.column     = src.col;
    addr.abs_column = src.abs_col;

    return addr;
}

range_t to_range(const formula_name_t::range_type& src)
{
    range_t range;
    range.first = to_address(src.first);
    range.last = to_address(src.last);

    return range;
}

formula_name_resolver::formula_name_resolver() {}
formula_name_resolver::~formula_name_resolver() {}

namespace {

string get_column_name_a1(col_t col)
{
    ostringstream os;
    append_column_name_a1(os, col);
    return os.str();
}

class excel_a1 : public formula_name_resolver
{
public:
    excel_a1(const iface::formula_model_access* cxt) : formula_name_resolver(), mp_cxt(cxt) {}
    virtual ~excel_a1() {}

    virtual formula_name_t resolve(const char* p, size_t n, const abs_address_t& pos) const
    {
        formula_name_t ret;
        if (!n)
            return ret;

        if (resolve_function(p, n, ret))
            return ret;

        if (resolve_table(mp_cxt, p, n, ret))
            return ret;

        const char* p_last = p;
        std::advance(p_last, n-1);

        // Use the sheet where the cell is unless sheet name is explicitly given.
        address_t parsed_addr(pos.sheet, 0, 0, false, false, false);

        parse_address_result_type parse_res = parse_address_excel_a1(mp_cxt, p, p_last, parsed_addr);

        if (parse_res != invalid)
        {
            // This is a valid A1-style address syntax-wise.

            if (parsed_addr.sheet == invalid_sheet)
                // sheet name is not found in the model.  Report back as invalid.
                return ret;

            if (!check_address_by_sheet_bounds(mp_cxt, parsed_addr))
                parse_res = invalid;
        }

        // prevent for example H to be recognized as column address
        if (parse_res == valid_address && parsed_addr.row != row_unset)
        {
            // This is a single cell address.
            to_relative_address(parsed_addr, pos, true);
            set_cell_reference(ret, parsed_addr);

            return ret;
        }

        if (parse_res == range_expected)
        {
            if (p == p_last)
                // ':' occurs as the last character.  This is not allowed.
                return ret;

            ++p; // skip ':'

            to_relative_address(parsed_addr, pos, true);
            set_address(ret.range.first, parsed_addr);

            // For now, we assume the sheet index of the end address is identical
            // to that of the begin address.
            parse_res = parse_address_excel_a1(NULL, p, p_last, parsed_addr);
            if (parse_res != valid_address)
                // The 2nd part after the ':' is not valid.
                return ret;

            to_relative_address(parsed_addr, pos, true);
            set_address(ret.range.last, parsed_addr);
            ret.range.last.sheet = ret.range.first.sheet; // re-use the sheet index of the begin address.
            ret.type = formula_name_t::range_reference;
            return ret;
        }

        resolve_function_or_name(p, n, ret);
        return ret;
    }

    virtual string get_name(const address_t& addr, const abs_address_t& pos, bool sheet_name) const
    {
        ostringstream os;
        append_address_a1(os, sheet_name ? mp_cxt : nullptr, addr, pos, '!');
        return os.str();
    }

    virtual string get_name(const range_t& range, const abs_address_t& pos, bool sheet_name) const
    {
        // For now, sheet index of the end-range address is ignored.

        ostringstream os;
        col_t col = range.first.column;
        row_t row = range.first.row;
        sheet_t sheet = range.first.sheet;

        if (!range.first.abs_sheet)
            sheet += pos.sheet;

        if (sheet_name && mp_cxt)
        {
            append_sheet_name(os, *mp_cxt, sheet);
            os << '!';
        }

        append_column_address_a1(os, col, pos.column, range.first.abs_column);
        append_row_address_a1(os, row, pos.row, range.first.abs_row);

        os << ":";
        col = range.last.column;
        row = range.last.row;

        append_column_address_a1(os, col, pos.column, range.last.abs_column);

        if (row != row_unset)
        {
            if (range.last.abs_row)
                os << '$';
            else
                row += pos.row;
            os << (row + 1);
        }

        return os.str();
    }

    virtual string get_name(const table_t& table) const
    {
        return to_string(mp_cxt, table);
    }

    virtual string get_column_name(col_t col) const
    {
        return get_column_name_a1(col);
    }

private:
    const iface::formula_model_access* mp_cxt;
};

class excel_r1c1 : public formula_name_resolver
{
    const iface::formula_model_access* mp_cxt;

    void write_sheet_name(ostringstream& os, const address_t& addr, const abs_address_t& pos) const
    {
        if (mp_cxt)
        {
            sheet_t sheet = addr.sheet;
            if (!addr.abs_sheet)
                sheet += pos.sheet;

            append_sheet_name(os, *mp_cxt, sheet);
            os << '!';
        }
    }

public:
    excel_r1c1(const iface::formula_model_access* cxt) : mp_cxt(cxt) {}

    virtual formula_name_t resolve(const char* p, size_t n, const abs_address_t& pos) const
    {
        formula_name_t ret;
        if (!n)
            return ret;

        if (resolve_function(p, n, ret))
            return ret;

        const char* p_end = p + n;
        const char* p_last = p;
        std::advance(p_last, n-1);

        // Use the sheet where the cell is unless sheet name is explicitly given.
        address_t parsed_addr(pos.sheet, 0, 0);

        parse_address_result_type parse_res = parse_address_excel_r1c1(mp_cxt, p, p_last, parsed_addr);

        if (parse_res != invalid)
        {
            // This is a valid R1C1-style address syntax-wise.

            if (parsed_addr.sheet == invalid_sheet)
                // sheet name is not found in the model.  Report back as invalid.
                return ret;

            if (!check_address_by_sheet_bounds(mp_cxt, parsed_addr))
                parse_res = invalid;
        }

        switch (parse_res)
        {
            case parse_address_result_type::valid_address:
            {
                set_cell_reference(ret, parsed_addr);
                return ret;
            }
            case parse_address_result_type::range_expected:
            {
                ++p; // skip ':'
                if (p == p_end)
                    return ret;

                address_t parsed_addr2(0, 0, 0);
                parse_address_result_type parse_res2 = parse_address_excel_r1c1(nullptr, p, p_last, parsed_addr2);
                if (parse_res2 != parse_address_result_type::valid_address)
                    return ret;

                // For now, we assume the sheet index of the end address is identical
                // to that of the begin address.
                parsed_addr2.sheet = parsed_addr.sheet;

                set_address(ret.range.first, parsed_addr);
                set_address(ret.range.last, parsed_addr2);
                ret.type = formula_name_t::range_reference;
                return ret;
            }
            default:
                ;
        }

        resolve_function_or_name(p, n, ret);
        return ret;
    }

    virtual std::string get_name(const address_t& addr, const abs_address_t& pos, bool sheet_name) const
    {
        ostringstream os;

        if (sheet_name)
            write_sheet_name(os, addr, pos);

        append_address_r1c1(os, addr, pos);
        return os.str();
    }

    virtual std::string get_name(const range_t& range, const abs_address_t& pos, bool sheet_name) const
    {
        ostringstream os;

        if (sheet_name)
            write_sheet_name(os, range.first, pos);

        append_address_r1c1(os, range.first, pos);
        os << ':';
        append_address_r1c1(os, range.last, pos);
        return os.str();
    }

    virtual std::string get_name(const table_t& table) const
    {
        return to_string(mp_cxt, table);
    }

    virtual std::string get_column_name(col_t col) const
    {
        ostringstream os;
        os << (col+1);
        return os.str();
    }
};

class dot_a1_resolver : public formula_name_resolver
{
    using func_parse_address_type =
        std::function<parse_address_result(
            const ixion::iface::formula_model_access*,
            const char*&, const char*, address_t&)>;

    using func_append_address_type =
        std::function<void(
            std::ostringstream&, const ixion::iface::formula_model_access*,
            const address_t&, const abs_address_t&, char)>;

    using func_append_sheet_name_type =
        std::function<void(
            std::ostringstream&, const ixion::iface::formula_model_access*,
            const address_t&, const abs_address_t&)>;

    const iface::formula_model_access* mp_cxt;
    func_parse_address_type m_func_parse_address;
    func_append_address_type m_func_append_address;
    func_append_sheet_name_type m_func_append_sheet_name;

    static bool display_last_sheet(const range_t& range, const abs_address_t& pos)
    {
        if (range.first.abs_sheet != range.last.abs_sheet)
            return true;

        abs_range_t abs = range.to_abs(pos);
        return abs.first.sheet != abs.last.sheet;
    }

public:
    dot_a1_resolver(
        const iface::formula_model_access* cxt,
        func_parse_address_type func_parse_address,
        func_append_address_type func_append_address,
        func_append_sheet_name_type func_append_sheet_name) :
        formula_name_resolver(),
        mp_cxt(cxt),
        m_func_parse_address(func_parse_address),
        m_func_append_address(func_append_address),
        m_func_append_sheet_name(func_append_sheet_name) {}

    virtual formula_name_t resolve(const char *p, size_t n, const abs_address_t &pos) const override
    {
        formula_name_t ret;
        if (!n)
            return ret;

        if (resolve_function(p, n, ret))
            return ret;

        const char* p_last = p + n -1;

        // Use the sheet where the cell is unless sheet name is explicitly given.
        address_t parsed_addr(pos.sheet, 0, 0, false, false, false);

        parse_address_result parse_res = m_func_parse_address(mp_cxt, p, p_last, parsed_addr);
        IXION_TRACE("parse address result on 1st address (" << to_string(parse_res.result) << ")");

        if (parse_res.result != invalid)
        {
            // This is a valid A1-style address syntax-wise.

            if (parsed_addr.sheet == invalid_sheet)
            {
                IXION_DEBUG("Sheet name is not found in the model context.");
                return ret;
            }

            if (!check_address_by_sheet_bounds(mp_cxt, parsed_addr))
            {
                IXION_DEBUG("Address is outside the sheet bounds.");
                parse_res.result = invalid;
            }
        }

        // prevent for example H to be recognized as column address
        if (parse_res.result == valid_address && parsed_addr.row != row_unset)
        {
            // This is a single cell address.
            to_relative_address(parsed_addr, pos, true);
            set_cell_reference(ret, parsed_addr);

            return ret;
        }

        if (parse_res.result == range_expected)
        {
            if (p == p_last)
            {
                IXION_DEBUG("':' occurs as the last character.  This is not allowed.");
                return ret;
            }

            ++p; // skip ':'

            to_relative_address(parsed_addr, pos, true);
            set_address(ret.range.first, parsed_addr);

            parse_res = m_func_parse_address(mp_cxt, p, p_last, parsed_addr);
            IXION_TRACE("parse address result on 2nd address (" << to_string(parse_res.result) << ")");

            if (parse_res.result != valid_address)
            {
                IXION_DEBUG("2nd part after the ':' is not valid.");
                return ret;
            }

            to_relative_address(parsed_addr, pos, parse_res.sheet_name);
            set_address(ret.range.last, parsed_addr);
            ret.type = formula_name_t::range_reference;
            return ret;
        }

        resolve_function_or_name(p, n, ret);
        return ret;
    }

    virtual std::string get_name(const address_t &addr, const abs_address_t &pos, bool sheet_name) const override
    {
        std::ostringstream os;
        if (sheet_name && addr.abs_sheet)
            os << '$';
        m_func_append_address(os, sheet_name ? mp_cxt : nullptr, addr, pos, '.');
        return os.str();
    }

    virtual std::string get_name(const range_t& range, const abs_address_t& pos, bool sheet_name) const override
    {
        ostringstream os;
        col_t col = range.first.column;
        row_t row = range.first.row;

        m_func_append_sheet_name(os, sheet_name ? mp_cxt : nullptr, range.first, pos);

        append_column_address_a1(os, col, pos.column, range.first.abs_column);
        append_row_address_a1(os, row, pos.row, range.first.abs_row);

        os << ":";

        if (!display_last_sheet(range, pos))
            sheet_name = false;

        m_func_append_sheet_name(os, sheet_name ? mp_cxt : nullptr, range.last, pos);

        col = range.last.column;
        row = range.last.row;

        append_column_address_a1(os, col, pos.column, range.last.abs_column);
        append_row_address_a1(os, row, pos.row, range.last.abs_row);

        return os.str();
    }

    virtual std::string get_name(const table_t &table) const override
    {
        // TODO : find out how Calc A1 handles table reference.
        return std::string();
    }

    virtual std::string get_column_name(col_t col) const override
    {
        return get_column_name_a1(col);
    }
};

/**
 * Name resolver for ODFF formula expressions.
 */
class odff_resolver : public formula_name_resolver
{
public:
    odff_resolver(const iface::formula_model_access* cxt) : formula_name_resolver(), mp_cxt(cxt) {}
    virtual ~odff_resolver() {}

    virtual formula_name_t resolve(const char* p, size_t n, const abs_address_t& pos) const
    {
        formula_name_t ret;

        if (resolve_function(p, n, ret))
            return ret;

        if (!n)
            // Empty string.
            return ret;

        // First character must be '[' for this to be a reference.
        if (*p != '[')
        {
            ret.type = formula_name_t::named_expression;
            return ret;
        }

        ++p;
        const char* p_last = p;
        std::advance(p_last, n-2);

        // Last character must be ']'.
        if (*p_last != ']')
            return ret;

        --p_last;

        // Use the sheet where the cell is unless sheet name is explicitly given.
        address_t parsed_addr(pos.sheet, 0, 0, true, false, false);

        parse_address_result parse_res = parse_address_odff(mp_cxt, p, p_last, parsed_addr);

        // prevent for example H to be recognized as column address
        if (parse_res.result == valid_address && parsed_addr.row != row_unset)
        {
            // This is a single cell address.
            to_relative_address(parsed_addr, pos, true);
            set_cell_reference(ret, parsed_addr);
            return ret;
        }

        if (parse_res.result == range_expected)
        {
            if (p == p_last)
                // ':' occurs as the last character.  This is not allowed.
                return ret;

            ++p; // skip ':'

            to_relative_address(parsed_addr, pos, true);
            set_address(ret.range.first, parsed_addr);

            parse_res = parse_address_odff(mp_cxt, p, p_last, parsed_addr);
            if (parse_res.result != valid_address)
                // The 2nd part after the ':' is not valid.
                return ret;

            to_relative_address(parsed_addr, pos, parse_res.sheet_name);

            set_address(ret.range.last, parsed_addr);
            ret.type = formula_name_t::range_reference;
            return ret;
        }

        resolve_function_or_name(p, n, ret);

        return ret;
    }

    virtual string get_name(const address_t& addr, const abs_address_t& pos, bool sheet_name) const
    {
        if (!mp_cxt)
            sheet_name = false;

        ostringstream os;
        os << '[';
        if (sheet_name)
        {
            if (addr.abs_sheet)
                os << '$';
            append_address_a1(os, mp_cxt, addr, pos, '.');
        }
        else
        {
            os << '.';
            append_address_a1(os, nullptr, addr, pos, '.');
        }
        os << ']';
        return os.str();
    }

    virtual string get_name(const range_t& range, const abs_address_t& pos, bool sheet_name) const
    {
        if (!mp_cxt)
            sheet_name = false;

        ostringstream os;
        os << '[';

        if (sheet_name)
        {
            const iface::formula_model_access* cxt = mp_cxt;

            if (range.first.abs_sheet)
                os << '$';
            append_address_a1(os, cxt, range.first, pos, '.');

            os << ':';

            if (range.last.sheet != range.first.sheet || range.last.abs_sheet != range.first.abs_sheet)
            {
                // sheet indices differ between the first and last addresses.
                if (range.last.abs_sheet)
                    os << '$';
            }
            else
            {
                // sheet indices are identical.
                cxt = nullptr;
                os << '.';
            }

            append_address_a1(os, cxt, range.last, pos, '.');
        }
        else
        {
            os << '.';
            append_address_a1(os, nullptr, range.first, pos, '.');
            os << ":.";
            append_address_a1(os, nullptr, range.last, pos, '.');
        }

        os << ']';
        return os.str();
    }

    virtual string get_name(const table_t& table) const
    {
        // TODO : ODF doesn't support table reference yet.
        return string();
    }

    virtual string get_column_name(col_t col) const
    {
        return get_column_name_a1(col);
    }

private:
    const iface::formula_model_access* mp_cxt;
};

}

std::unique_ptr<formula_name_resolver> formula_name_resolver::get(
    formula_name_resolver_t type, const iface::formula_model_access* cxt)
{

    switch (type)
    {
        case formula_name_resolver_t::excel_a1:
            return std::unique_ptr<formula_name_resolver>(new excel_a1(cxt));
        case formula_name_resolver_t::excel_r1c1:
            return std::unique_ptr<formula_name_resolver>(new excel_r1c1(cxt));
        case formula_name_resolver_t::odff:
            return std::unique_ptr<formula_name_resolver>(new odff_resolver(cxt));
        case formula_name_resolver_t::calc_a1:
            return std::unique_ptr<formula_name_resolver>(
                new dot_a1_resolver(
                    cxt, parse_address_calc_a1, append_address_a1, append_sheet_name_calc_a1));
        case formula_name_resolver_t::odf_cra:
            return std::unique_ptr<formula_name_resolver>(
                new dot_a1_resolver(
                    cxt, parse_address_odf_cra, append_address_a1_with_sheet_name_sep, append_sheet_name_odf_cra));
        case formula_name_resolver_t::unknown:
        default:
            ;
    }

    return std::unique_ptr<formula_name_resolver>();
}

}
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
