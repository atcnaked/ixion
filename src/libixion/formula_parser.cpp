/*************************************************************************
 *
 * Copyright (c) 2010 Kohei Yoshida
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

#include "formula_parser.hpp"
#include "formula_functions.hpp"
#include "hash_container/map.hpp"

#include <iostream>
#include <sstream>

#define DEBUG_PARSER 1

using namespace std;

namespace ixion {

namespace {

class ref_error : public general_error
{
public:
    ref_error(const string& msg) :
        general_error(msg) {}
};

class formula_token_printer : public unary_function<formula_token_base, void>
{
public:
    formula_token_printer(const cell_name_ptr_map_t& cell_names)
    {
        // Construct a cell pointer to name association.
        cell_name_ptr_map_t::const_iterator itr = cell_names.begin(), itr_end = cell_names.end();
        for (; itr != itr_end; ++itr)
        {
            m_cell_ptr_name_map.insert(
                cell_ptr_name_map_t::value_type(itr->second, itr->first));
        }
    }

    void operator() (const formula_token_base& token) const
    {
        fopcode_t oc = token.get_opcode();   
        ostringstream os;
        os << " ";
        os << "<" << get_opcode_name(oc) << ">'";

        switch (oc)
        {
            case fop_close:
                os << ")";
                break;
            case fop_divide:
                os << "/";
                break;
            case fop_err_no_ref:
                break;
            case fop_minus:
                os << "-";
                break;
            case fop_multiply:
                os << "*";
                break;
            case fop_open:
                os << "(";
                break;
            case fop_plus:
                os << "+";
                break;
            case fop_sep:
                os << ",";
                break;
            case fop_single_ref:
                os << get_cell_name(token.get_single_ref());
                break;
            case fop_unresolved_ref:
                os << token.get_name();
                break;
            case fop_string:
                break;
            case fop_value:
                os << token.get_value();
                break;
            case fop_function:
            {
                formula_function_t func_oc = formula_functions::get_function_opcode(token);
                os << formula_functions::get_function_name(func_oc);
            }
            break;
            default:
                ;
        }
        os << "'";
#if DEBUG_PARSER
        cout << os.str();
#endif
    }

private:
    string get_cell_name(const base_cell* p) const
    {
        cell_ptr_name_map_t::const_iterator itr = m_cell_ptr_name_map.find(p);
        if (itr == m_cell_ptr_name_map.end())
            return string();
        return itr->second;
    }

private:
    cell_ptr_name_map_t m_cell_ptr_name_map;
};

}

// ----------------------------------------------------------------------------

formula_parser::parse_error::parse_error(const string& msg) :
    general_error(msg) {}

// ----------------------------------------------------------------------------

formula_parser::formula_parser(const string& name, const lexer_tokens_t& tokens, cell_name_ptr_map_t* p_cell_names,
                               bool ignore_unresolved) :
    m_name(name),
    m_tokens(tokens),
    mp_cell_names(p_cell_names),
    m_ignore_unresolved(ignore_unresolved)
{
}

formula_parser::~formula_parser()
{
}

void formula_parser::parse()
{
    try
    {
        size_t n = m_tokens.size();
        for (size_t i = 0; i < n; ++i)
        {
            const lexer_token_base& t = m_tokens[i];
            lexer_opcode_t oc = t.get_opcode();
            switch (oc)
            {
                case op_open:
                case op_close:
                case op_plus:
                case op_minus:
                case op_multiply:
                case op_divide:
                case op_sep:
                    primitive(oc);
                    break;
                case op_name:
                    name(t);
                    break;
                case op_string:
                    break;
                case op_value:
                    value(t);
                    break;
                default:
                    ;
            }
        }
    }
    catch (const ref_error& e)
    {
#if DEBUG_PARSER
        cout << "reference error: " << e.what() << endl;
#endif
    }
    catch (const parse_error& e)
    {
#if DEBUG_PARSER
        cout << "parse error: " << e.what() << endl;
#endif
    }
}

void formula_parser::print_tokens() const
{
#if DEBUG_PARSER
    cout << "formula tokens:";
    for_each(m_formula_tokens.begin(), m_formula_tokens.end(), formula_token_printer(*mp_cell_names));
    cout << endl;
#endif
}

formula_tokens_t& formula_parser::get_tokens()
{
    return m_formula_tokens;
}

const vector<base_cell*>& formula_parser::get_depend_cells() const
{
    return m_depend_cells;
}

void formula_parser::primitive(lexer_opcode_t oc)
{
    fopcode_t foc = fop_unknown;
    switch (oc)
    {
        case op_close:
            foc = fop_close;
            break;
        case op_divide:
            foc = fop_divide;
            break;
        case op_minus:
            foc = fop_minus;
            break;
        case op_multiply:
            foc = fop_multiply;
            break;
        case op_open:
            foc = fop_open;
            break;
        case op_plus:
            foc = fop_plus;
            break;
        case op_sep:
            foc = fop_sep;
            break;
        default:
            throw parse_error("unknown primitive token received");
    }
    m_formula_tokens.push_back(new opcode_token(foc));
}

void formula_parser::name(const lexer_token_base& t)
{
    const string name = t.get_string();
    cell_name_ptr_map_t::iterator itr = mp_cell_names->find(name);
    if (itr != mp_cell_names->end())
    {
        // This is a reference, either to a cell or to a name.
#if DEBUG_PARSER
        cout << "  name = " << name << "  pointer to the cell instance = " << itr->second << endl;
#endif
        m_formula_tokens.push_back(new single_ref_token(itr->second));
        m_depend_cells.push_back(itr->second);
        return;
    }

    // Check if this is a function name.
    formula_function_t func_oc = formula_functions::get_function_opcode(name);
    if (func_oc != func_unknown)
    {
#if DEBUG_PARSER
        cout << "'" << name << "' is a built-in function." << endl;
#endif
        m_formula_tokens.push_back(new function_token(static_cast<size_t>(func_oc)));
        return;
    }

    if ( m_ignore_unresolved )
    {
        m_formula_tokens.push_back(new unresolved_ref_token(name));
        // Not added to the dependent cells as it's unresolved
        return;
    }

    ostringstream os;
    os << "failed to resolve a name '" << name << "'.";
    throw parse_error(os.str());
}

void formula_parser::value(const lexer_token_base& t)
{
    double val = t.get_value();
    m_formula_tokens.push_back(new value_token(val));
}

}
