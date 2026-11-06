/*************************************************************************
*  Copyright (C) 2004 by Olivier Galizzi                                 *
*  Copyright (C) 2004-2021 by Janek Kozicki                              *
*                                                                        *
*  This program is free software; it is licensed under the terms of the  *
*  GNU General Public License v2 or later. See file LICENSE for details. *
*************************************************************************/

#pragma once

#include "ClassFactory.hpp"

#include <boost/enable_shared_from_this.hpp>
#include <string>

namespace yade { // Cannot have #include directive inside.

namespace detail {
	string factorableBaseClassName(const char* baseClasses, unsigned int index);
	int    factorableBaseClassNumber(const char* baseClasses);
}

//! macro for registering both class and its base
#define REGISTER_CLASS_AND_BASE(cn, bcn)                                                                                                                       \
	REGISTER_CLASS_NAME(cn);                                                                                                                               \
	REGISTER_BASE_CLASS_NAME(bcn);

#define REGISTER_CLASS_NAME(cn)                                                                                                                                \
public:                                                                                                                                                        \
	string getClassName() const override { return #cn; };

// FIXME[1] - that macro below should go to another class! factorable has nothing to do with inheritance tree.

#define REGISTER_BASE_CLASS_NAME(bcn)                                                                                                                          \
public:                                                                                                                                                        \
	string getBaseClassName(unsigned int iTokenBaseClass = 0) const override { return ::yade::detail::factorableBaseClassName(#bcn, iTokenBaseClass); }    \
                                                                                                                                                               \
public:                                                                                                                                                        \
	int getBaseClassNumber() const override { return ::yade::detail::factorableBaseClassNumber(#bcn); }


class Factorable : public boost::enable_shared_from_this<Factorable> {
public:
	Factorable() { }
	virtual ~Factorable() { }

	virtual string getBaseClassName(unsigned int = 0) const { return ""; }
	virtual int    getBaseClassNumber() const { return 0; }
	virtual string getClassName() const { return "Factorable"; };
};

} // namespace yade
