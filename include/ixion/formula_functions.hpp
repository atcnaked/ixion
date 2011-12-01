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

#ifndef __IXION_FORMULA_FUNCTIONS_HPP__
#define __IXION_FORMULA_FUNCTIONS_HPP__

#include "ixion/global.hpp"
#include "ixion/exceptions.hpp"
#include "ixion/formula_function_opcode.hpp"

#include <string>
#include <vector>

namespace ixion {

class formula_token_base;

namespace iface {

class model_context;

}

class formula_functions
{
public:
    class invalid_arg : public general_error
    {
    public:
        invalid_arg(const ::std::string& msg);
    };

    formula_functions(iface::model_context& cxt);
    ~formula_functions();

    static formula_function_t get_function_opcode(const formula_token_base& token);
    static formula_function_t get_function_opcode(const char* p, size_t n);
    static const char* get_function_name(formula_function_t oc);

    void interpret(formula_function_t oc, value_stack_t& args);

private:
    void max(value_stack_t& args) const;
    void min(value_stack_t& args) const;
    void sum(value_stack_t& args) const;
    void average(value_stack_t& args) const;

    void len(value_stack_t& args) const;
    void concatenate(value_stack_t& args);

    void wait(value_stack_t& args) const;

private:
    iface::model_context& m_context;
};

}

#endif
