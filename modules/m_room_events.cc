// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::mapi::header
IRCD_MODULE
{
	"Matrix room library"
};

//
// tools
//

std::pair<int64_t, ircd::m::event::idx>
IRCD_MODULE_EXPORT
ircd::m::twain(const room &room)
{
	std::pair<int64_t, m::event::idx> ret
	{
		-1, 0
	};

	const room::events::sounding s
	{
		room
	};

	s.rfor_each([&ret]
	(const auto &range, const auto &event_idx)
	{
		ret.first = range.first - 1;
		ret.second = event_idx;
		return false;
	});

	return ret;
}

std::pair<int64_t, ircd::m::event::idx>
IRCD_MODULE_EXPORT
ircd::m::sounding(const room &room)
{
	std::pair<int64_t, m::event::idx> ret
	{
		-1, 0
	};

	const room::events::sounding s
	{
		room
	};

	s.rfor_each([&ret]
	(const auto &range, const auto &event_idx)
	{
		ret.first = range.second;
		ret.second = event_idx;
		return false;
	});

	return ret;
}

std::pair<int64_t, ircd::m::event::idx>
IRCD_MODULE_EXPORT
ircd::m::hazard(const room &room)
{
	std::pair<int64_t, m::event::idx> ret
	{
		0, 0
	};

	const room::events::sounding s
	{
		room
	};

	s.for_each([&ret]
	(const auto &range, const auto &event_idx)
	{
		ret.first = range.first;
		ret.second = event_idx;
		return false;
	});

	return ret;
}

//
// room::events
//

size_t
IRCD_MODULE_EXPORT
ircd::m::room::events::count(const m::event::id &a,
                             const m::event::id &b)
{
	return count(index(a), index(b));
}

size_t
IRCD_MODULE_EXPORT
ircd::m::room::events::count(const m::event::idx &a,
                             const m::event::idx &b)
{
	// Get the room_id from b here; a might not be in the same room but downstream
	// the counter seeks to a in the given room and will properly fail there.
	room::id::buf room_id
	{
		m::get(std::max(a, b), "room_id", room_id)
	};

	return count(room_id, a, b);
}

size_t
IRCD_MODULE_EXPORT
ircd::m::room::events::count(const m::room &room,
                             const m::event::id &a,
                             const m::event::id &b)
{
	return count(room, index(a), index(b));
}

size_t
IRCD_MODULE_EXPORT
ircd::m::room::events::count(const m::room &room,
                             const m::event::idx &a,
                             const m::event::idx &b)
{
	m::room::events it
	{
		room
	};

	assert(a <= b);
	it.seek_idx(a);

	if(!it && !exists(room))
		throw m::NOT_FOUND
		{
			"Cannot find room '%s' to count events in",
			string_view{room.room_id}
		};
	else if(!it)
		throw m::NOT_FOUND
		{
			"Event @ idx:%lu or idx:%lu not found in room '%s' or at all",
			a,
			b,
			string_view{room.room_id}
		};

	size_t ret{0};
	// Hit the iterator once first otherwise the count will always increment
	// to `1` erroneously when it ought to show `0`.
	for(++it; it && it.event_idx() < b; ++it, ++ret);
	return ret;
}

//
// room::events::events
//
// (see: ircd/m_room.cc for now)

//
// room::events::missing
//

bool
IRCD_MODULE_EXPORT
ircd::m::room::events::missing::for_each(const closure &closure)
const
{
	return true;
}

//
// room::events::sounding
//

bool
IRCD_MODULE_EXPORT
ircd::m::room::events::sounding::rfor_each(const closure &closure)
const
{
	room::events it
	{
		room
	};

	if(!it)
		return true;

	event::idx idx{0};
	for(sounding::range range{0L, it.depth()}; it; --it)
	{
		range.first = it.depth();
		if(range.first == range.second)
		{
			idx = it.event_idx();
			continue;
		}

		--range.second;
		if(range.first == range.second)
		{
			idx = it.event_idx();
			continue;
		}

		if(!closure({range.first+1, range.second+1}, idx))
			return false;

		range.second = range.first;
	}

	return true;
}

bool
IRCD_MODULE_EXPORT
ircd::m::room::events::sounding::for_each(const closure &closure)
const
{
	room::events it
	{
		room, int64_t(0L)
	};

	for(sounding::range range{0L, 0L}; it; ++it)
	{
		range.second = it.depth();
		if(range.first == range.second)
			continue;

		++range.first;
		if(range.first == range.second)
			continue;

		if(!closure(range, it.event_idx()))
			return false;

		range.first = range.second;
	}

	return true;
}
