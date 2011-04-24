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

#ifndef __IXION_GLOBAL_HPP__
#define __IXION_GLOBAL_HPP__

#include "ixion/hash_container/map.hpp"
#include "ixion/hash_container/set.hpp"

#include <string>
#include <boost/ptr_container/ptr_map.hpp>

namespace ixion {

typedef int col_t;
typedef int row_t;
typedef int sheet_t;

class base_cell;
class formula_cell;

typedef _ixion_unordered_map_type<const base_cell*, ::std::string> cell_ptr_name_map_t;

/**
 * Dirty cells are those cells that have been modified or cells that 
 * reference modified cells.  Note that dirty cells can be of any cell 
 * types. 
 */
typedef _ixion_unordered_set_type<base_cell*> dirty_cells_t;

const char* get_formula_result_output_separator();


// ============================================================================

class global
{
public:
    static ::std::string get_cell_name(const base_cell* cell);

    /**
     * Get current time in seconds since epoch.  Note that the value 
     * representing a time may differ from platform to platform.  Use this 
     * value only to measure relative time. 
     * 
     * @return current time in seconds since epoch.
     */
    static double get_current_time();

    /**
     * Suspend execution of the calling thread for specified seconds.
     * 
     * @param seconds duration of sleep.
     */
    static void sleep(unsigned int seconds);

    /**
     * Load the entire content of a file on disk.  When loading, this function 
     * appends an extra character to the end, the pointer to which can be used 
     * as the position past the last character.
     * 
     * @param filepath path of the file to load. 
     *  
     * @param content string instance to pass the file content to.
     */
    static void load_file_content(const ::std::string& filepath, ::std::string& content);

private:
    global();
    global(const global& r);
    ~global();
};

// ============================================================================

class general_error : public ::std::exception
{
public:
    explicit general_error(const ::std::string& msg);
    ~general_error() throw();
    virtual const char* what() const throw();
private:
    ::std::string m_msg;
};

// ============================================================================

class file_not_found : public ::std::exception
{
public:
    explicit file_not_found(const ::std::string& fpath);
    ~file_not_found() throw();
    virtual const char* what() const throw();
private:
    ::std::string m_fpath;
};

// ============================================================================

enum formula_error_t
{
    fe_no_error = 0,
    fe_ref_result_not_available,
    fe_division_by_zero,
    fe_invalid_expression
};

const char* get_formula_error_name(formula_error_t fe);

// ============================================================================

class formula_error : public ::std::exception
{
public:
    explicit formula_error(formula_error_t fe);
    ~formula_error() throw();
    virtual const char* what() const throw();

    formula_error_t get_error() const;
private:
    formula_error_t m_ferror;
};

}

#endif