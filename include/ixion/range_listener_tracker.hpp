/*************************************************************************
 *
 * Copyright (c) 2011 Kohei Yoshida
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

#ifndef __IXION_RANGE_LISTENER_TRACKER_HPP__
#define __IXION_RANGE_LISTENER_TRACKER_HPP__

#include "ixion/global.hpp"
#include "ixion/address.hpp"
#include "ixion/model_context.hpp"

#include <mdds/rectangle_set.hpp>

namespace ixion {

/**
 * Track all range references being listened to by cells.
 */
class range_listener_tracker
{
    typedef ::std::vector<abs_address_t> address_list_type;
    typedef ::mdds::rectangle_set<row_t, address_list_type> range_set_type;

    range_listener_tracker(); // disabled
public:
    range_listener_tracker(model_context& cxt);
    ~range_listener_tracker();

    void add(const abs_address_t& cell, const abs_range_t& range);
    void remove(const abs_address_t& cell, const abs_range_t& range);

    /**
     * Given a modified cell (target), get all formula cells that need to be 
     * re-calculated based on the range-to-cells listener relationships. 
     * 
     * @param target 
     * @param listeners 
     */
    void get_all_listeners(const abs_address_t& target, dirty_cells_t& listeners) const;
private:
    model_context& m_context;
    range_set_type m_ranges;
};

}

#endif
