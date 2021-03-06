// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::mapi::header
IRCD_MODULE
{
	"Matrix room head"
};

//
// room::head
//

std::pair<ircd::json::array, int64_t>
IRCD_MODULE_EXPORT
ircd::m::room::head::generate(const mutable_buffer &buf,
                              const size_t &limit,
                              const bool &need_top)
const
{
	json::stack out{buf};
	json::stack::array array{out};
	const auto depth
	{
		generate(array, limit, need_top)
	};
	array.~array();
	return
	{
		json::array{out.completed()},
		depth
	};
}

int64_t
IRCD_MODULE_EXPORT
ircd::m::room::head::generate(json::stack::array &out,
                              const size_t &_limit,
                              const bool &_need_tophead)
const
{
	const m::event::id::closure &v1_ref{[&out]
	(const auto &event_id)
	{
		json::stack::array prev{out};
		prev.append(event_id);
		{
			json::stack::object nilly{prev};
			json::stack::member willy
			{
				nilly, "", ""
			};
		}
	}};

	const m::event::id::closure &v3_ref{[&out]
	(const auto &event_id)
	{
		out.append(event_id);
	}};

	char versionbuf[32];
	const auto version
	{
		m::version(versionbuf, room, std::nothrow)
	};

	const auto &append
	{
		version == "1" || version == "2"? v1_ref : v3_ref
	};

	bool need_tophead{_need_tophead};
	const auto top_head
	{
		need_tophead?
			m::top(std::nothrow, room.room_id):
			std::tuple<m::id::event::buf, int64_t, m::event::idx>{}
	};

	int64_t depth{-1};
	size_t limit{_limit};
	m::event::fetch event;
	for_each([&event, &need_tophead, &top_head, &depth, &append, &limit]
	(const m::event::idx &idx, const m::event::id &event_id)
	{
		if(!seek(event, idx, event_id, std::nothrow))
			return true;

		if(need_tophead)
			if(event.event_id == std::get<0>(top_head))
				need_tophead = false;

		append(event_id);
		depth = std::max(json::get<"depth"_>(event), depth);
		return --limit - need_tophead > 0;
	});

	if(need_tophead)
	{
		append(std::get<0>(top_head));
		depth = std::get<1>(top_head);
	}

	return depth;
}

size_t
IRCD_MODULE_EXPORT
ircd::m::room::head::count()
const
{
	size_t ret(0);
	for_each([&ret]
	(const event::idx &event_idx, const event::id &event_id)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
IRCD_MODULE_EXPORT
ircd::m::room::head::has(const event::id &event_id)
const
{
	bool ret{false};
	for_each([&ret, &event_id]
	(const event::idx &event_idx, const event::id &event_id_)
	{
		ret = event_id_ == event_id;
		return !ret; // for_each protocol: false to break
	});

	return ret;
}

bool
IRCD_MODULE_EXPORT
ircd::m::room::head::for_each(const closure &closure)
const
{
	auto it
	{
		dbs::room_head.begin(room.room_id)
	};

	for(; it; ++it)
	{
		const event::id &event_id
		{
			dbs::room_head_key(it->first)
		};

		const event::idx &event_idx
		{
			byte_view<event::idx>{it->second}
		};

		if(!closure(event_idx, event_id))
			return false;
	}

	return true;
}

//
// special tools
//

size_t
IRCD_MODULE_EXPORT
ircd::m::room::head::reset(const head &head)
{
	size_t ret{0};
	const auto &room{head.room};
	m::room::events it{room};
	if(!it)
		return ret;

	// Replacement will be the single new head
	const m::event replacement
	{
		*it
	};

	db::txn txn
	{
		*m::dbs::events
	};

	// Iterate all of the existing heads with a delete operation
	m::dbs::write_opts opts;
	opts.op = db::op::DELETE;
	opts.appendix.reset();
	opts.appendix.set(dbs::appendix::ROOM_HEAD);
	head.for_each([&room, &opts, &txn, &ret]
	(const m::event::idx &event_idx, const m::event::id &event_id)
	{
		const m::event::fetch event
		{
			event_idx, std::nothrow
		};

		if(!event.valid)
		{
			log::derror
			{
				m::log, "Invalid event '%s' idx %lu in head for %s",
				string_view{event_id},
				event_idx,
				string_view{room.room_id}
			};

			return true;
		}

		opts.event_idx = event_idx;
		m::dbs::write(txn, event, opts);
		++ret;
		return true;
	});

	// Finally add the replacement to the txn
	opts.op = db::op::SET;
	opts.event_idx = it.event_idx();
	m::dbs::write(txn, replacement, opts);

	// Commit txn
	txn();
	return ret;
}

size_t
IRCD_MODULE_EXPORT
ircd::m::room::head::rebuild(const head &head)
{
	size_t ret{0};
	static const m::event::fetch::opts fopts
	{
		{ db::get::NO_CACHE }
	};

	m::room::events it
	{
		head.room, 0UL, &fopts
	};

	if(!it)
		return ret;

	db::txn txn
	{
		*m::dbs::events
	};

	m::dbs::write_opts opts;
	opts.op = db::op::SET;
	for(; it; ++it)
	{
		const m::event &event{*it};
		opts.event_idx = it.event_idx();
		opts.appendix.reset();
		opts.appendix.set(dbs::appendix::ROOM_HEAD);
		m::dbs::write(txn, event, opts);
		++ret;
	}

	txn();
	return ret;
}

void
IRCD_MODULE_EXPORT
ircd::m::room::head::modify(const m::event::id &event_id,
                            const db::op &op,
                            const bool &refs)
{
	const m::event::fetch event
	{
		event_id
	};

	db::txn txn
	{
		*m::dbs::events
	};

	// Iterate all of the existing heads with a delete operation
	m::dbs::write_opts opts;
	opts.op = op;
	opts.event_idx = event.event_idx;
	opts.appendix.reset();
	opts.appendix.set(dbs::appendix::ROOM_HEAD);
	m::dbs::write(txn, event, opts);

	// Commit txn
	txn();
}
