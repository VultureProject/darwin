/// \file     uuid.hpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     12/08/20
/// \license  GPLv3
/// \brief    Copyright (c) 2020 Advens. All rights reserved.

#include <boost/uuid/random_generator.hpp>

#include "Uuid.hpp"

std::vector<char> darwin::uuid::GenUuid() {
    boost::uuids::random_generator gen;
    boost::uuids::uuid u = gen();

    std::vector<char> v(u.size());
    std::copy(u.begin(), u.end(), v.begin());
    return v;
}