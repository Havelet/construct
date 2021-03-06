// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_CTX_THIS_CTX_H

/// Interface to the currently running context
namespace ircd::ctx {
inline namespace this_ctx
{
	struct ctx &cur() noexcept;                  ///< Assumptional reference to *current
	const uint64_t &id() noexcept;               // Unique ID for cur ctx
	string_view name() noexcept;                 // Optional label for cur ctx
	ulong cycles() noexcept;                     // misc profiling related
	bool interruption_requested() noexcept;      // interruption(cur())

	void interruption_point();                   // throws if interruption_requested()
	void wait();                                 // Returns when context is woken up.
	void yield();                                // Allow other contexts to run before returning.

	// Return remaining time if notified; or <= 0 if not, and timeout thrown on throw overloads
	microseconds wait(const microseconds &, const std::nothrow_t &);
	template<class E, class duration> nothrow_overload<E, duration> wait(const duration &);
	template<class E = timeout, class duration> throw_overload<E, duration> wait(const duration &);

	// Returns false if notified; true if time point reached, timeout thrown on throw_overloads
	bool wait_until(const system_point &tp, const std::nothrow_t &);
	template<class E> nothrow_overload<E, bool> wait_until(const system_point &tp);
	template<class E = timeout> throw_overload<E> wait_until(const system_point &tp);

	// Ignores notes. Throws if interrupted.
	void sleep_until(const system_point &tp);
	template<class duration> void sleep(const duration &);
	void sleep(const int &secs);
}}

namespace ircd::ctx
{
	/// Points to the currently running context or null for main stack (do not modify)
	extern __thread ctx *current;
}

namespace ircd
{
	namespace this_ctx = ctx::this_ctx;
}

/// This overload matches ::sleep() and acts as a drop-in for ircd contexts.
/// interruption point.
inline void
ircd::ctx::this_ctx::sleep(const int &secs)
{
	sleep(seconds(secs));
}

/// Yield the context for a period of time and ignore notifications. sleep()
/// is like wait() but it only returns after the timeout and not because of a
/// note.
/// interruption point.
template<class duration>
void
ircd::ctx::this_ctx::sleep(const duration &d)
{
	sleep_until(system_clock::now() + d);
}

/// Wait for a notification until a point in time. If there is a notification
/// then context continues normally. If there's never a notification then an
/// exception (= timeout) is thrown.
/// interruption point.
template<class E>
ircd::throw_overload<E>
ircd::ctx::this_ctx::wait_until(const system_point &tp)
{
	if(wait_until<std::nothrow_t>(tp))
		throw E{};
}

/// Wait for a notification until a point in time. If there is a notification
/// then returns true. If there's never a notification then returns false.
/// interruption point. this is not noexcept.
template<class E>
ircd::nothrow_overload<E, bool>
ircd::ctx::this_ctx::wait_until(const system_point &tp)
{
	return wait_until(tp, std::nothrow);
}

/// Wait for a notification for at most some amount of time. If the duration is
/// reached without a notification then E (= timeout) is thrown. Otherwise,
/// returns the time remaining on the duration.
/// interruption point
template<class E,
         class duration>
ircd::throw_overload<E, duration>
ircd::ctx::this_ctx::wait(const duration &d)
{
	const auto ret
	{
		wait<std::nothrow_t>(d)
	};

	return ret <= duration(0)?
		throw E{}:
		ret;
}

/// Wait for a notification for some amount of time. This function returns
/// when a context is notified. It always returns the duration remaining which
/// will be <= 0 to indicate a timeout without notification.
/// interruption point. this is not noexcept.
template<class E,
         class duration>
ircd::nothrow_overload<E, duration>
ircd::ctx::this_ctx::wait(const duration &d)
{
	const auto ret
	{
		wait(duration_cast<microseconds>(d), std::nothrow)
	};

	return duration_cast<duration>(ret);
}

/// View the name of the currently running context, or "*" if no context is
/// currently running.
inline ircd::string_view
ircd::ctx::this_ctx::name()
noexcept
{
	return current? name(cur()) : "*"_sv;
}

/// Calculate the current TSC (reference cycle count) accumulated for this
/// context only. This is done by first calculating a cycle count for the
/// current slice/epoch (see: ctx/prof.h) which is where the RDTSC sample
/// occurs. This count is added to an accumulator value saved in the ctx
/// structure. The accumulator value is updated at the end of each execution
/// slice, thus giving us the cycle count for this ctx only, up to this point.
extern inline ulong
__attribute__((flatten, always_inline, gnu_inline, artificial))
ircd::ctx::this_ctx::cycles()
noexcept
{
	const auto slice(prof::cur_slice_cycles());
	const auto accumulated(cycles(cur()));
	return accumulated + slice;
}

/// Reference to the currently running context. Call if you expect to be in a
/// context. Otherwise use the ctx::current pointer.
inline ircd::ctx::ctx &
__attribute__((always_inline))
ircd::ctx::this_ctx::cur()
noexcept
{
	assert(current);
	return *current;
}
