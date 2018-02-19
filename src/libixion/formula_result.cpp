/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ixion/formula_result.hpp"
#include "ixion/mem_str_buf.hpp"
#include "ixion/exceptions.hpp"
#include "ixion/interface/formula_model_access.hpp"
#include "ixion/config.hpp"

#include <cassert>
#include <sstream>
#include <iomanip>

#define DEBUG_FORMULA_RESULT 0

#if DEBUG_FORMULA_RESULT
#include <iostream>
#endif

using namespace std;

namespace ixion {

struct formula_result::impl
{
    result_type m_type;
    union {
        string_id_t m_str_identifier;
        double m_value;
        formula_error_t m_error;
    };

    impl() : m_type(result_type::value), m_value(0.0) {}
    impl(double v) : m_type(result_type::value), m_value(v) {}
    impl(string_id_t strid) : m_type(result_type::string), m_str_identifier(strid) {}
    impl(formula_error_t e) : m_type(result_type::error), m_error(e) {}

    impl(const impl& other) : m_type(other.m_type)
    {
        switch (m_type)
        {
            case result_type::value:
                m_value = other.m_value;
            break;
            case result_type::string:
                m_str_identifier = other.m_str_identifier;
            break;
            case result_type::error:
                m_error = other.m_error;
            break;
            default:
                assert(!"unknown formula result type specified during copy construction.");
        }
    }

    void reset()
    {
        m_type = result_type::value;
        m_value = 0.0;
    }

    void set_value(double v)
    {
        m_type = result_type::value;
        m_value = v;
    }

    void set_string(string_id_t strid)
    {
        m_type = result_type::string;
        m_str_identifier = strid;
    }

    void set_error(formula_error_t e)
    {
        m_type = result_type::error;
        m_error = e;
    }

    double get_value() const
    {
        assert(m_type == result_type::value);
        return m_value;
    }

    string_id_t get_string() const
    {
        assert(m_type == result_type::string);
        return m_str_identifier;
    }

    formula_error_t get_error() const
    {
        assert(m_type == result_type::error);
        return m_error;
    }

    result_type get_type() const
    {
        return m_type;
    }

    string str(const iface::formula_model_access& cxt) const
    {
        switch (m_type)
        {
            case result_type::error:
                return string(get_formula_error_name(m_error));
            case result_type::string:
            {
                const string* p = cxt.get_string(m_str_identifier);
                return p ? *p : string();
            }
            case result_type::value:
            {
                ostringstream os;
                if (cxt.get_config().output_precision >= 0)
                    os << std::fixed << std::setprecision(cxt.get_config().output_precision);
                os << m_value;
                return os.str();
            }
            default:
                assert(!"unknown formula result type!");
        }
        return string();
    }

    void parse(iface::formula_model_access& cxt, const char* p, size_t n)
    {
        if (!n)
            return;

        switch (*p)
        {
            case '#':
            {
                parse_error(p, n);
                break;
            }
            case '"':
            {
                parse_string(cxt, p, n);
                break;
            }
            case 't':
            case 'f':
            {
                // parse this as a boolean value.
                m_value = global::to_bool(p, n) ? 1.0 : 0.0;
                m_type = result_type::value;
                break;
            }
            default:
            {
                // parse this as a number.
                m_value = global::to_double(p, n);
                m_type = result_type::value;
            }
        }
    }

    void assign_from(const formula_result& r)
    {
        m_type = r.mp_impl->m_type;
        switch (m_type)
        {
            case result_type::value:
                m_value = r.mp_impl->m_value;
            break;
            case result_type::string:
                m_str_identifier = r.mp_impl->m_str_identifier;
            break;
            case result_type::error:
                m_error = r.mp_impl->m_error;
            break;
            default:
                assert(!"unknown formula result type specified during copy construction.");
        }
    }

    bool equals(const formula_result& r) const
    {
        if (m_type != r.mp_impl->m_type)
            return false;

        switch (m_type)
        {
            case result_type::value:
                return m_value == r.mp_impl->m_value;
            break;
            case result_type::string:
                return m_str_identifier == r.mp_impl->m_str_identifier;
            break;
            case result_type::error:
                return m_error == r.mp_impl->m_error;
            break;
            default:
                assert(!"unknown formula result type specified during copy construction.");
        }

        assert(!"this should never be reached!");
        return false;
    }

    void parse_error(const char* p, size_t n)
    {
        assert(n);
        assert(*p == '#');

        const char* p0 = p;
        const char* p_end = p + n;

        ++p; // skip '#'.
        mem_str_buf buf;
        for (; p != p_end; ++p)
        {
            bool good = true;

            switch (*p)
            {
                case '!':
                {
                    if (buf.empty())
                    {
                        good = false;
                        break;
                    }

                    if (buf.equals("REF"))
                    {
                        m_error = formula_error_t::ref_result_not_available;
                    }
                    else if (buf.equals("DIV/0"))
                    {
                        m_error = formula_error_t::division_by_zero;
                    }
                    else
                    {
                        good = false;
                        break;
                    }

                    m_type = result_type::error;
                    return;
                }
                case '?':
                {
                    if (buf.empty())
                    {
                        good = false;
                        break;
                    }

                    if (buf.equals("NAME"))
                        m_error = formula_error_t::name_not_found;
                    else
                    {
                        good = false;
                        break;
                    }

                    m_type = result_type::error;
                    return;
                }
            }

            if (!good)
                // parse failure.
                break;

            if (buf.empty())
                buf.set_start(p);
            else
                buf.inc();
        }

        ostringstream os;
        os << "malformed error string: " << string(p0, n);
        throw general_error(os.str());
    }

    void parse_string(iface::formula_model_access& cxt, const char* p, size_t n)
    {
        if (n <= 1)
            return;

        assert(*p == '"');
        ++p;
        const char* p_first = p;
        size_t len = 0;
        for (size_t i = 1; i < n; ++i, ++len, ++p)
        {
            char c = *p;
            if (c == '"')
                break;
        }

        if (!len)
            throw general_error("failed to parse string result.");

        m_type = result_type::string;
        m_str_identifier = cxt.add_string(p_first, len);
    }
};

formula_result::formula_result() :
    mp_impl(ixion::make_unique<impl>()) {}

formula_result::formula_result(const formula_result& r) :
    mp_impl(ixion::make_unique<impl>(*r.mp_impl)) {}

formula_result::formula_result(double v) : mp_impl(ixion::make_unique<impl>(v)) {}

formula_result::formula_result(string_id_t strid) : mp_impl(ixion::make_unique<impl>(strid)) {}

formula_result::formula_result(formula_error_t e) : mp_impl(ixion::make_unique<impl>(e)) {}

formula_result::~formula_result() {}

void formula_result::reset()
{
    mp_impl->reset();
}

void formula_result::set_value(double v)
{
    mp_impl->set_value(v);
}

void formula_result::set_string(string_id_t strid)
{
    mp_impl->set_string(strid);
}

void formula_result::set_error(formula_error_t e)
{
    mp_impl->set_error(e);
}

double formula_result::get_value() const
{
    return mp_impl->get_value();
}

string_id_t formula_result::get_string() const
{
    return mp_impl->get_string();
}

formula_error_t formula_result::get_error() const
{
    return mp_impl->get_error();
}

formula_result::result_type formula_result::get_type() const
{
    return mp_impl->get_type();
}

string formula_result::str(const iface::formula_model_access& cxt) const
{
    return mp_impl->str(cxt);
}

void formula_result::parse(iface::formula_model_access& cxt, const char* p, size_t n)
{
    mp_impl->parse(cxt, p, n);
}

formula_result& formula_result::operator= (const formula_result& r)
{
    mp_impl->assign_from(r);
    return *this;
}

bool formula_result::operator== (const formula_result& r) const
{
    return mp_impl->equals(r);
}

bool formula_result::operator!= (const formula_result& r) const
{
    return !operator== (r);
}

}
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
