// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_PROF_SCOPE_CYCLES_H

namespace ircd::prof
{
	struct scope_cycles;
}

/// Count the reference cycles for a scope using the lifetime of this object.
/// The result is stored in `result`. Note that `result` is also used while
/// this object is operating.
struct ircd::prof::scope_cycles
{
	uint64_t &result;

	scope_cycles(uint64_t &result) noexcept;
	~scope_cycles() noexcept;
};

extern inline
__attribute__((flatten, always_inline, gnu_inline, artificial))
ircd::prof::scope_cycles::scope_cycles(uint64_t &result)
noexcept
:result{result}
{
	#if defined(__x86_64__) || defined(__i386__)
	asm volatile ("mfence");
	asm volatile ("lfence");
	#endif

	result = cycles();

	#if defined(__x86_64__) || defined(__i386__)
	asm volatile ("lfence");
	#endif
}

extern inline
__attribute__((flatten, always_inline, gnu_inline, artificial))
ircd::prof::scope_cycles::~scope_cycles()
noexcept
{
	#if defined(__x86_64__) || defined(__i386__)
	asm volatile ("mfence");
	asm volatile ("lfence");
	#endif

	result = cycles() - result;

	#if defined(__x86_64__) || defined(__i386__)
	asm volatile ("lfence");
	#endif
}
