/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_FORMULA_TOKENS_HPP
#define INCLUDED_FORMULA_TOKENS_HPP

#include "ixion/address.hpp"
#include "ixion/table.hpp"
#include "ixion/formula_opcode.hpp"
#include "ixion/formula_tokens_fwd.hpp"

#include <string>

namespace ixion {

/**
 * Get a printable name for a formula opcode.
 *
 * @param oc formula opcode
 *
 * @return printable name for a formula opcode.
 */
IXION_DLLPUBLIC const char* get_opcode_name(fopcode_t oc);

IXION_DLLPUBLIC const char* get_formula_opcode_string(fopcode_t oc);

class IXION_DLLPUBLIC formula_token
{
    fopcode_t m_opcode;

public:
    formula_token() = delete;

    formula_token(fopcode_t op);
    formula_token(const formula_token& r);
    virtual ~formula_token() = 0;

    fopcode_t get_opcode() const;

    bool operator== (const formula_token& r) const;
    bool operator!= (const formula_token& r) const;

    virtual address_t get_single_ref() const;
    virtual range_t get_range_ref() const;
    virtual table_t get_table_ref() const;
    virtual double get_value() const;
    virtual size_t get_index() const;
    virtual std::string get_name() const;
    virtual void write_string(std::ostream& os) const;
};

class IXION_DLLPUBLIC formula_tokens_store
{
    friend void intrusive_ptr_add_ref(formula_tokens_store*);
    friend void intrusive_ptr_release(formula_tokens_store*);

    struct impl;
    std::unique_ptr<impl> mp_impl;

    void add_ref();
    void release_ref();

    formula_tokens_store();

public:

    static formula_tokens_store_ptr_t create();

    ~formula_tokens_store();

    formula_tokens_store(const formula_tokens_store&) = delete;
    formula_tokens_store& operator= (const formula_tokens_store&) = delete;

    size_t get_reference_count() const;

    formula_tokens_t& get();
    const formula_tokens_t& get() const;
};

inline void intrusive_ptr_add_ref(formula_tokens_store* p)
{
    p->add_ref();
}

inline void intrusive_ptr_release(formula_tokens_store* p)
{
    p->release_ref();
}

IXION_DLLPUBLIC bool operator== (const formula_tokens_t& left, const formula_tokens_t& right);

IXION_DLLPUBLIC std::ostream& operator<< (std::ostream& os, const formula_token& ft);

}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
