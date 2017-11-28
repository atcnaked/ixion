/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ixion/info.hpp"

#include "constants.inl"

namespace ixion {

int get_version_major()
{
    return IXION_MAJOR_VERSION;
}

int get_version_minor()
{
    return IXION_MINOR_VERSION;
}

int get_version_micro()
{
    return IXION_MICRO_VERSION;
}

int get_api_version_major()
{
    return IXION_MAJOR_API_VERSION;
}

int get_api_version_minor()
{
    return IXION_MINOR_API_VERSION;
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
