// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

size_t
ircd::m::room::purge(const room &room)
{
	size_t ret(0);
	db::txn txn
	{
		*m::dbs::events
	};

	room.for_each([&txn, &ret]
	(const m::event::idx &idx)
	{
		const m::event::fetch event
		{
			idx
		};

		m::dbs::write_opts opts;
		opts.op = db::op::DELETE;
		opts.event_idx = idx;
		m::dbs::write(txn, event, opts);
		++ret;
	});

	txn();
	return ret;
}

ircd::m::room::state::rebuild::rebuild(const room::id &room_id)
{
	const m::event::id::buf event_id
	{
		m::head(room_id)
	};

	const m::room::state::history history
	{
		room_id, event_id
	};

	const m::room::state present_state
	{
		room_id
	};

	const bool check_auth
	{
		!m::internal(room_id)
	};

	m::dbs::write_opts opts;
	opts.appendix.reset();
	opts.appendix.set(dbs::appendix::ROOM_STATE);
	opts.appendix.set(dbs::appendix::ROOM_JOINED);
	db::txn txn
	{
		*m::dbs::events
	};

	ssize_t deleted(0);
	present_state.for_each([&opts, &txn, &deleted]
	(const auto &type, const auto &state_key, const auto &event_idx)
	{
		const m::event::fetch &event
		{
			event_idx, std::nothrow
		};

		if(!event.valid)
			return true;

		auto _opts(opts);
		_opts.op = db::op::DELETE;
		_opts.event_idx = event_idx;
		dbs::write(txn, event, _opts);
		++deleted;
		return true;
	});

	ssize_t added(0);
	history.for_each([&opts, &txn, &added, &room_id, &check_auth]
	(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx)
	{
		const m::event::fetch &event
		{
			event_idx, std::nothrow
		};

		if(!event.valid)
			return true;

		const auto &[pass, fail]
		{
			check_auth?
				auth::check_present(event):
				room::auth::passfail{true, {}}
		};

		if(!pass)
		{
			log::dwarning
			{
				log, "%s fails for present state in %s :%s",
				string_view{event.event_id},
				string_view{room_id},
				what(fail),
			};

			return true;
		}

		auto _opts(opts);
		_opts.op = db::op::SET;
		_opts.event_idx = event_idx;
		dbs::write(txn, event, _opts);
		++added;
		return true;
	});

	log::info
	{
		log, "Present state of %s @ %s rebuild complete with %zu size:%s del:%zd add:%zd (%zd)",
		string_view{room_id},
		string_view{event_id},
		txn.size(),
		pretty(iec(txn.bytes())),
		deleted,
		added,
		(added - deleted),
	};

	txn();
}

bool
ircd::m::room::state::is(const event::idx &event_idx)
{
	bool ret{false};
	m::get(event_idx, "state_key", [&ret]
	(const string_view &state_key)
	{
		ret = true;
	});

	return ret;
}

bool
ircd::m::room::state::is(std::nothrow_t,
                         const event::idx &event_idx)
{
	bool ret{false};
	m::get(std::nothrow, event_idx, "state_key", [&ret]
	(const string_view &state_key)
	{
		ret = true;
	});

	return ret;
}

size_t
ircd::m::room::state::purge_replaced(const room::id &room_id)
{
	db::txn txn
	{
		*m::dbs::events
	};

	size_t ret(0);
	m::room::events it
	{
		room_id, uint64_t(0)
	};

	if(!it)
		return ret;

	for(; it; ++it)
	{
		const m::event::idx &event_idx(it.event_idx());
		if(!m::get(std::nothrow, event_idx, "state_key", [](const auto &) {}))
			continue;

		if(!m::event::refs(event_idx).count(m::dbs::ref::NEXT_STATE))
			continue;

		// TODO: erase event
	}

	return ret;
}

bool
ircd::m::room::state::present(const event::idx &event_idx)
{
	static const event::fetch::opts fopts
	{
		event::keys::include { "room_id", "type", "state_key" },
	};

	const m::event::fetch event
	{
		event_idx, fopts
	};

	const m::room room
	{
		at<"room_id"_>(event)
	};

	const m::room::state state
	{
		room
	};

	const auto state_idx
	{
		state.get(std::nothrow, at<"type"_>(event), at<"state_key"_>(event))
	};

	assert(event_idx);
	return state_idx == event_idx;
}

ircd::m::event::idx
ircd::m::room::state::prev(const event::idx &event_idx)
{
	event::idx ret{0};
	prev(event_idx, [&ret]
	(const event::idx &event_idx)
	{
		if(event_idx > ret)
			ret = event_idx;

		return true;
	});

	return ret;
}

ircd::m::event::idx
ircd::m::room::state::next(const event::idx &event_idx)
{
	event::idx ret{0};
	next(event_idx, [&ret]
	(const event::idx &event_idx)
	{
		if(event_idx > ret)
			ret = event_idx;

		return true;
	});

	return ret;
}

bool
ircd::m::room::state::next(const event::idx &event_idx,
                           const event::closure_idx_bool &closure)
{
	const m::event::refs refs
	{
		event_idx
	};

	return refs.for_each(dbs::ref::NEXT_STATE, [&closure]
	(const event::idx &event_idx, const dbs::ref &ref)
	{
		assert(ref == dbs::ref::NEXT_STATE);
		return closure(event_idx);
	});
}

bool
ircd::m::room::state::prev(const event::idx &event_idx,
                           const event::closure_idx_bool &closure)
{
	const m::event::refs refs
	{
		event_idx
	};

	return refs.for_each(dbs::ref::PREV_STATE, [&closure]
	(const event::idx &event_idx, const dbs::ref &ref)
	{
		assert(ref == dbs::ref::PREV_STATE);
		return closure(event_idx);
	});
}

ircd::m::room
ircd::m::create(const id::room &room_id,
                const id::user &creator,
                const string_view &preset)
{
	return create(createroom
	{
		{ "room_id",  room_id },
		{ "creator",  creator },
		{ "preset",   preset  },
	});
}

ircd::m::room
ircd::m::create(const createroom &c,
                json::stack::array *const &errors)
{
	using prototype = room (const createroom &, json::stack::array *const &);

	static mods::import<prototype> call
	{
		"m_room_create", "ircd::m::create"
	};

	return call(c, errors);
}

ircd::m::event::id::buf
ircd::m::join(const id::room_alias &room_alias,
              const id::user &user_id)
{
	using prototype = event::id::buf (const id::room_alias &, const id::user &);

	static mods::import<prototype> function
	{
		"m_room_join", "ircd::m::join"
	};

	return function(room_alias, user_id);
}

ircd::m::event::id::buf
ircd::m::join(const room &room,
              const id::user &user_id)
{
	using prototype = event::id::buf (const m::room &, const id::user &);

	static mods::import<prototype> function
	{
		"m_room_join", "ircd::m::join"
	};

	return function(room, user_id);
}

ircd::m::event::id::buf
ircd::m::leave(const room &room,
               const id::user &user_id)
{
	using prototype = event::id::buf (const m::room &, const id::user &);

	static mods::import<prototype> function
	{
		"m_room_leave", "ircd::m::leave"
	};

	return function(room, user_id);
}

ircd::m::event::id::buf
ircd::m::invite(const room &room,
                const id::user &target,
                const id::user &sender)
{
	json::iov content;
	return invite(room, target, sender, content);
}

ircd::m::event::id::buf
ircd::m::invite(const room &room,
                const id::user &target,
                const id::user &sender,
                json::iov &content)
{
	using prototype = event::id::buf (const m::room &, const id::user &, const id::user &, json::iov &);

	static mods::import<prototype> call
	{
		"client_rooms", "ircd::m::invite"
	};

	return call(room, target, sender, content);
}

ircd::m::event::id::buf
ircd::m::redact(const room &room,
                const id::user &sender,
                const id::event &event_id,
                const string_view &reason)
{
	json::iov event;
	const json::iov::push push[]
	{
		{ event,    { "type",       "m.room.redaction"  }},
		{ event,    { "sender",      sender             }},
		{ event,    { "redacts",     event_id           }},
	};

	json::iov content;
	const json::iov::set _reason
	{
		content, !empty(reason),
		{
			"reason", [&reason]() -> json::value
			{
				return reason;
			}
		}
	};

	return commit(room, event, content);
}

ircd::m::event::id::buf
ircd::m::notice(const room &room,
                const string_view &body)
{
	return message(room, me.user_id, body, "m.notice");
}

ircd::m::event::id::buf
ircd::m::notice(const room &room,
                const m::id::user &sender,
                const string_view &body)
{
	return message(room, sender, body, "m.notice");
}

ircd::m::event::id::buf
ircd::m::msghtml(const room &room,
                 const m::id::user &sender,
                 const string_view &html,
                 const string_view &alt,
                 const string_view &msgtype)
{
	return message(room, sender,
	{
		{ "msgtype",         msgtype                       },
		{ "format",          "org.matrix.custom.html"      },
		{ "body",            { alt?: html, json::STRING }  },
		{ "formatted_body",  { html, json::STRING }        },
	});
}

ircd::m::event::id::buf
ircd::m::message(const room &room,
                 const m::id::user &sender,
                 const string_view &body,
                 const string_view &msgtype)
{
	return message(room, sender,
	{
		{ "body",     { body,    json::STRING } },
		{ "msgtype",  { msgtype, json::STRING } },
	});
}

ircd::m::event::id::buf
ircd::m::message(const room &room,
                 const m::id::user &sender,
                 const json::members &contents)
{
	return send(room, sender, "m.room.message", contents);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="
ircd::m::event::id::buf
__attribute__((stack_protect))
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::members &contents)
{
	const size_t contents_count
	{
		std::min(contents.size(), json::object::max_sorted_members)
	};

	json::iov _content;
	json::iov::push content[contents_count]; // 48B each
	return send(room, sender, type, state_key, make_iov(_content, content, contents_count, contents));
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="
ircd::m::event::id::buf
__attribute__((stack_protect))
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::object &contents)
{
	const size_t contents_count
	{
		std::min(contents.size(), json::object::max_sorted_members)
	};

	json::iov _content;
	json::iov::push content[contents_count]; // 48B each
	return send(room, sender, type, state_key, make_iov(_content, content, contents_count, contents));
}
#pragma GCC diagnostic pop

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::iov &content)
{
	json::iov event;
	const json::iov::push push[]
	{
		{ event,    { "sender",     sender     }},
		{ event,    { "type",       type       }},
		{ event,    { "state_key",  state_key  }},
	};

	return commit(room, event, content);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="
ircd::m::event::id::buf
__attribute__((stack_protect))
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const json::members &contents)
{
	const size_t contents_count
	{
		std::min(contents.size(), json::object::max_sorted_members)
	};

	json::iov _content;
	json::iov::push content[contents_count]; // 48B each
	return send(room, sender, type, make_iov(_content, content, contents_count, contents));
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="
ircd::m::event::id::buf
__attribute__((stack_protect))
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const json::object &contents)
{
	const size_t contents_count
	{
		std::min(contents.size(), json::object::max_sorted_members)
	};

	json::iov _content;
	json::iov::push content[contents_count]; // 48B each
	return send(room, sender, type, make_iov(_content, content, contents_count, contents));
}
#pragma GCC diagnostic pop

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const id::user &sender,
              const string_view &type,
              const json::iov &content)
{
	json::iov event;
	const json::iov::push push[]
	{
		{ event,    { "sender",  sender  }},
		{ event,    { "type",    type    }},
	};

	return commit(room, event, content);
}

ircd::m::event::id::buf
ircd::m::commit(const room &room,
                json::iov &event,
                const json::iov &contents)
{
	// Set the room_id on the iov
	json::iov::push room_id
	{
		event, { "room_id", room.room_id }
	};

	vm::copts opts
	{
		room.copts?
			*room.copts:
			vm::default_copts
	};

	// Some functionality on this server may create an event on behalf
	// of remote users. It's safe for us to mask this here, but eval'ing
	// this event in any replay later will require special casing.
	opts.non_conform |= event::conforms::MISMATCH_ORIGIN_SENDER;

	// Don't need this here
	opts.verify = false;

	return vm::eval
	{
		event, contents, opts
	};
}

ircd::m::id::room::buf
ircd::m::room_id(const id::room_alias &room_alias)
{
	char buf[m::id::MAX_SIZE + 1];
	static_assert(sizeof(buf) <= 256);
	return room_id(buf, room_alias);
}

ircd::m::id::room::buf
ircd::m::room_id(const id::event &event_id)
{
	char buf[m::id::MAX_SIZE + 1];
	static_assert(sizeof(buf) <= 256);
	return room_id(buf, event_id);
}

ircd::m::id::room::buf
ircd::m::room_id(const string_view &mxid)
{
	char buf[m::id::MAX_SIZE + 1];
	static_assert(sizeof(buf) <= 256);
	return room_id(buf, mxid);
}

ircd::m::id::room
ircd::m::room_id(const mutable_buffer &out,
                 const string_view &mxid)
{
	switch(m::sigil(mxid))
	{
		case id::ROOM:
			return id::room{out, mxid};

		case id::USER:
		{
			const m::user::room user_room(mxid);
			return string_view{data(out), copy(out, user_room.room_id)};
		}

		case id::NODE:
		{
			const m::node node(lstrip(mxid, ':'));
			return node.room_id(out);
		}

		case id::EVENT:
			return room_id(out, id::event{mxid});

		default:
			return room_id(out, id::room_alias{mxid});
	}
}

ircd::m::id::room
ircd::m::room_id(const mutable_buffer &out,
                 const id::event &event_id)
{
	room::id ret;
	m::get(event_id, "room_id", [&out, &ret]
	(const room::id &room_id)
	{
		ret = string_view { data(out), copy(out, room_id) };
	});

	return ret;
}

ircd::m::id::room
ircd::m::room_id(const mutable_buffer &out,
                 const id::room_alias &room_alias)
{
	room::id ret;
	room::aliases::cache::get(room_alias, [&out, &ret]
	(const room::id &room_id)
	{
		ret = string_view { data(out), copy(out, room_id) };
	});

	return ret;
}

bool
ircd::m::exists(const id::room_alias &room_alias,
                const bool &remote_query)
{
	if(room::aliases::cache::has(room_alias))
		return true;

	if(!remote_query)
		return false;

	return room::aliases::cache::get(std::nothrow, room_alias, [](const room::id &room_id) {});
}

int64_t
ircd::m::depth(const id::room &room_id)
{
	return std::get<int64_t>(top(room_id));
}

int64_t
ircd::m::depth(std::nothrow_t,
               const id::room &room_id)
{
	const auto it
	{
		dbs::room_events.begin(room_id)
	};

	if(!it)
		return -1;

	const auto part
	{
		dbs::room_events_key(it->first)
	};

	return std::get<0>(part);
}

ircd::m::event::idx
ircd::m::head_idx(const id::room &room_id)
{
	return std::get<event::idx>(top(room_id));
}

ircd::m::event::idx
ircd::m::head_idx(std::nothrow_t,
                  const id::room &room_id)
{
	const auto it
	{
		dbs::room_events.begin(room_id)
	};

	if(!it)
		return 0;

	const auto part
	{
		dbs::room_events_key(it->first)
	};

	return std::get<1>(part);
}

ircd::m::event::id::buf
ircd::m::head(const id::room &room_id)
{
	return std::get<event::id::buf>(top(room_id));
}

ircd::m::event::id::buf
ircd::m::head(std::nothrow_t,
              const id::room &room_id)
{
	return std::get<event::id::buf>(top(std::nothrow, room_id));
}

std::tuple<ircd::m::event::id::buf, int64_t, ircd::m::event::idx>
ircd::m::top(const id::room &room_id)
{
	const auto ret
	{
		top(std::nothrow, room_id)
	};

	if(std::get<int64_t>(ret) == -1)
		throw m::NOT_FOUND
		{
			"No head for room %s", string_view{room_id}
		};

	return ret;
}

std::tuple<ircd::m::event::id::buf, int64_t, ircd::m::event::idx>
ircd::m::top(std::nothrow_t,
             const id::room &room_id)
{
	const auto it
	{
		dbs::room_events.begin(room_id)
	};

	if(!it)
		return
		{
			event::id::buf{}, -1, 0
		};

	const auto part
	{
		dbs::room_events_key(it->first)
	};

	const int64_t &depth
	{
		int64_t(std::get<0>(part))
	};

	const event::idx &event_idx
	{
		std::get<1>(part)
	};

	std::tuple<event::id::buf, int64_t, event::idx> ret
	{
		event::id::buf{}, depth, event_idx
	};

	m::event_id(event_idx, std::nothrow, [&ret]
	(const event::id &event_id)
	{
		std::get<event::id::buf>(ret) = event_id;
	});

	return ret;
}

ircd::m::id::user::buf
ircd::m::any_user(const room &room,
                  const string_view &host,
                  const string_view &membership)
{
	user::id::buf ret;
	const room::members members{room};
	members.for_each(membership, [&host, &ret]
	(const auto &user_id, const auto &event_idx)
	{
		if(host && user_id.host() != host)
			return true;

		ret = user_id;
		return false;
	});

	return ret;
}

/// Receive the join_rule of the room into buffer of sufficient size.
/// The protocol does not specify a join_rule string longer than 7
/// characters but do be considerate of the future. This function
/// properly defaults the string as per the protocol spec.
ircd::string_view
ircd::m::join_rule(const mutable_buffer &out,
                   const room &room)
{
	static const string_view default_join_rule
	{
		"invite"
	};

	string_view ret
	{
		default_join_rule
	};

	const event::keys::include keys
	{
		"content"
	};

	const m::event::fetch::opts fopts
	{
		keys, room.fopts? room.fopts->gopts : db::gopts{}
	};

	const room::state state
	{
		room, &fopts
	};

	state.get(std::nothrow, "m.room.join_rules", "", [&ret, &out]
	(const m::event &event)
	{
		const auto &content
		{
			json::get<"content"_>(event)
		};

		const json::string &rule
		{
			content.get("join_rule", default_join_rule)
		};

		ret = string_view
		{
			data(out), copy(out, rule)
		};
	});

	return ret;
}

ircd::string_view
ircd::m::version(const mutable_buffer &buf,
                 const room &room)
{
	const auto ret
	{
		version(buf, room, std::nothrow)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"Failed to find room %s to query its version",
			string_view{room.room_id}
		};

	return ret;
}

ircd::string_view
ircd::m::version(const mutable_buffer &buf,
                 const room &room,
                 std::nothrow_t)
{
	const auto event_idx
	{
		room.get(std::nothrow, "m.room.create", "")
	};

	string_view ret
	{
		strlcpy{buf, "1"_sv}
	};

	m::get(std::nothrow, event_idx, "content", [&buf, &ret]
	(const json::object &content)
	{
		const json::string &version
		{
			content.get("room_version", "1")
		};

		ret = strlcpy
		{
			buf, version
		};
	});

	return ret;
}

ircd::string_view
ircd::m::type(const mutable_buffer &buf,
              const room &room)
{
	string_view ret;
	const auto event_idx
	{
		room.get(std::nothrow, "m.room.create", "")
	};

	m::get(std::nothrow, event_idx, "content", [&buf, &ret]
	(const json::object &content)
	{
		const json::string &type
		{
			content.get("type")
		};

		ret = strlcpy
		{
			buf, type
		};
	});

	return ret;
}

ircd::m::id::user::buf
ircd::m::creator(const id::room &room_id)
{
	// Query the sender field of the event to get the creator. This is for
	// future compatibility if the content.creator field gets eliminated.
	static const event::fetch::opts fopts
	{
		event::keys::include {"sender"}
	};

	const room::state state
	{
		room_id, &fopts
	};

	id::user::buf ret;
	state.get("m.room.create", "", [&ret]
	(const m::event &event)
	{
		ret = user::id
		{
			json::get<"sender"_>(event)
		};
	});

	return ret;
}

//
// boolean suite
//

/// The only members are from our origin, in any membership state. This
/// indicates we won't have any other federation servers that could possibly
/// be party to anything about this room.
bool
ircd::m::local_only(const room &room)
{
	// Branch to test if any remote users are joined to the room, meaning
	// this result must be false; this is a fast query.
	if(remote_joined(room))
		return false;

	const room::members members
	{
		room
	};

	return members.for_each([]
	(const id::user &user_id)
	{
		return my(user_id);
	});
}

/// Member(s) from our server are presently joined to the room. Returns false
/// if there's a room on the server where all of our users have left. Note that
/// some internal rooms have no memberships at all and this will also be false.
/// This can return true if other servers have memberships in the room too, as
/// long as one of our users is joined.
bool
ircd::m::local_joined(const room &room)
{
	const room::members members
	{
		room
	};

	return !members.empty("join", my_host());
}

/// Member(s) from another server are presently joined to the room. For example
/// if another user leaves a PM with our user who is still joined, this returns
/// false. This can return true even if the room has no memberships in any
/// state from our server, as long as there's a joined member from a remote.
bool
ircd::m::remote_joined(const room &room)
{
	const room::members members
	{
		room
	};

	return !members.for_each("join", []
	(const id::user &user_id)
	{
		return my(user_id)? true : false; // false to break.
	});
}

bool
ircd::m::visible(const room &room,
                 const string_view &mxid,
                 const event *const &event)
{
	if(event)
		return m::visible(*event, mxid);

	const m::event event_
	{
		json::members
		{
			{ "event_id",  room.event_id  },
			{ "room_id",   room.room_id   },
		}
	};

	return m::visible(event_, mxid);
}

/// Test of the join_rule of the room is the argument.
bool
ircd::m::join_rule(const room &room,
                   const string_view &rule)
{
	char buf[32];
	return join_rule(buf, room) == rule;
}

bool
ircd::m::creator(const room::id &room_id,
                 const user::id &user_id)
{
	const auto creator_user_id
	{
		creator(room_id)
	};

	return creator_user_id == user_id;
}

bool
ircd::m::federated(const id::room &room_id)
{
	static const m::event::fetch::opts fopts
	{
		event::keys::include { "content" },
	};

	const m::room::state state
	{
		room_id, &fopts
	};

	bool ret;
	state.get("m.room.create", "", [&ret]
	(const m::event &event)
	{
		ret = json::get<"content"_>(event).get("m.federate", true);
	});

	return ret;
}

/// Determine if this is an internal room. The following must be satisfied:
///
/// - The room was created by this origin.
/// - The creator was the server itself, not any other user.
bool
ircd::m::internal(const id::room &room_id)
{
	const m::room room
	{
		room_id
	};

	if(!my(room))
		return false;

	if(!exists(room))
		return false;

	if(!creator(room, m::me))
		return false;

	return true;
}

bool
ircd::m::exists(const id::room &room_id)
{
	const m::room::events it
	{
		room_id, 0UL
	};

	if(!it)
		return false;

	if(likely(it.depth() < 2UL))
		return true;

	if(my_host(room_id.host()) && creator(room_id, m::me))
		return true;

	return false;
}

bool
ircd::m::exists(const room &room)
{
	return exists(room.room_id);
}

//
// util
//

bool
ircd::m::operator==(const room &a, const room &b)
{
	return !(a != b);
}

bool
ircd::m::operator!=(const room &a_, const room &b_)
{
	const string_view &a{a_.room_id}, &b{b_.room_id};
	return a != b;
}

bool
ircd::m::operator!(const room &a)
{
	return !a.room_id;
}

bool
ircd::m::my(const room &room)
{
	return my(room.room_id);
}

//
// room
//

/// A room index is just the event::idx of its create event.
ircd::m::event::idx
ircd::m::room::index(const room::id &room_id)
{
	const auto ret
	{
		index(room_id, std::nothrow)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"No index for room %s", string_view{room_id}
		};

	return ret;
}

ircd::m::event::idx
ircd::m::room::index(const room::id &room_id,
                     std::nothrow_t)
{
	uint64_t depth{0};
	room::events it
	{
		room_id, depth
	};

	return it? it.event_idx() : 0;
}

//
// room::room
//

size_t
ircd::m::room::count()
const
{
	size_t ret(0);
	for_each(event::closure_idx_bool{[&ret]
	(const event::idx &event_idx)
	{
		++ret;
		return true;
	}});

	return ret;
}

size_t
ircd::m::room::count(const string_view &type)
const
{
	size_t ret(0);
	for_each(type, event::closure_idx_bool{[&ret]
	(const event::idx &event_idx)
	{
		++ret;
		return true;
	}});

	return ret;
}

size_t
ircd::m::room::count(const string_view &type,
                     const string_view &state_key)
const
{
	size_t ret(0);
	for_each(type, event::closure_idx_bool{[&state_key, &ret]
	(const event::idx &event_idx)
	{
		ret += query(std::nothrow, event_idx, "state_key", [&state_key]
		(const string_view &_state_key) -> bool
		{
			return state_key == _state_key;
		});

		return true;
	}});

	return ret;
}

bool
ircd::m::room::has(const string_view &type)
const
{
	return get(std::nothrow, type, event::closure{});
}

void
ircd::m::room::get(const string_view &type,
                   const event::closure &closure)
const
{
	if(!get(std::nothrow, type, closure))
		throw m::NOT_FOUND
		{
			"No events of type '%s' found in '%s'",
			type,
			room_id
		};
}

bool
ircd::m::room::get(std::nothrow_t,
                   const string_view &type,
                   const event::closure &closure)
const
{
	bool ret{false};
	for_each(type, event::closure_bool{[&ret, &closure]
	(const event &event)
	{
		if(closure)
			closure(event);

		ret = true;
		return false;
	}});

	return ret;
}

ircd::m::event::idx
ircd::m::room::get(const string_view &type)
const
{
	const event::idx ret
	{
		get(std::nothrow, type)
	};

	if(unlikely(!ret))
		throw m::NOT_FOUND
		{
			"No events of type '%s' found in '%s'",
			type,
			room_id
		};

	return ret;
}

ircd::m::event::idx
ircd::m::room::get(std::nothrow_t,
                   const string_view &type)
const
{
	event::idx ret{0};
	for_each(type, event::closure_idx_bool{[&ret]
	(const event::idx &event_idx)
	{
		ret = event_idx;
		return false;
	}});

	return ret;
}

ircd::m::event::idx
ircd::m::room::get(const string_view &type,
                   const string_view &state_key)
const
{
	return state(*this).get(type, state_key);
}

ircd::m::event::idx
ircd::m::room::get(std::nothrow_t,
                   const string_view &type,
                   const string_view &state_key)
const
{
	return state(*this).get(std::nothrow, type, state_key);
}

void
ircd::m::room::get(const string_view &type,
                   const string_view &state_key,
                   const event::closure &closure)
const
{
	const state state{*this};
	state.get(type, state_key, closure);
}

bool
ircd::m::room::get(std::nothrow_t,
                   const string_view &type,
                   const string_view &state_key,
                   const event::closure &closure)
const
{
	const state state{*this};
	return state.get(std::nothrow, type, state_key, closure);
}

bool
ircd::m::room::has(const string_view &type,
                   const string_view &state_key)
const
{
	const state state{*this};
	return state.has(type, state_key);
}

void
ircd::m::room::for_each(const event::closure &closure)
const
{
	for_each(string_view{}, closure);
}

bool
ircd::m::room::for_each(const event::closure_bool &closure)
const
{
	return for_each(string_view{}, closure);
}

void
ircd::m::room::for_each(const event::id::closure &closure)
const
{
	for_each(string_view{}, closure);
}

bool
ircd::m::room::for_each(const event::id::closure_bool &closure)
const
{
	return for_each(string_view{}, closure);
}

void
ircd::m::room::for_each(const event::closure_idx &closure)
const
{
	for_each(string_view{}, closure);
}

bool
ircd::m::room::for_each(const event::closure_idx_bool &closure)
const
{
	return for_each(string_view{}, closure);
}

void
ircd::m::room::for_each(const string_view &type,
                        const event::closure &closure)
const
{
	for_each(type, event::closure_bool{[&closure]
	(const event &event)
	{
		closure(event);
		return true;
	}});
}

bool
ircd::m::room::for_each(const string_view &type,
                        const event::closure_bool &closure)
const
{
	event::fetch event
	{
		fopts? *fopts : event::fetch::default_opts
	};

	return for_each(type, event::closure_idx_bool{[&closure, &event]
	(const event::idx &event_idx)
	{
		if(!seek(event, event_idx, std::nothrow))
			return true;

		return closure(event);
	}});
}

void
ircd::m::room::for_each(const string_view &type,
                        const event::id::closure &closure)
const
{
	for_each(type, event::id::closure_bool{[&closure]
	(const event::id &event_id)
	{
		closure(event_id);
		return true;
	}});
}

bool
ircd::m::room::for_each(const string_view &type,
                        const event::id::closure_bool &closure)
const
{
	return for_each(type, event::closure_idx_bool{[&closure]
	(const event::idx &idx)
	{
		bool ret{true};
		m::event_id(idx, std::nothrow, [&ret, &closure]
		(const event::id &event_id)
		{
			ret = closure(event_id);
		});

		return ret;
	}});
}

void
ircd::m::room::for_each(const string_view &type,
                        const event::closure_idx &closure)
const
{
	for_each(type, event::closure_idx_bool{[&closure]
	(const event::idx &idx)
	{
		closure(idx);
		return true;
	}});
}

bool
ircd::m::room::for_each(const string_view &type,
                        const event::closure_idx_bool &closure)
const
{
	static constexpr auto idx
	{
		json::indexof<event, "type"_>()
	};

	auto &column
	{
		dbs::event_column.at(idx)
	};

	events it{*this};
	for(; it; --it)
	{
		const auto &event_idx
		{
			it.event_idx()
		};

		bool match
		{
			empty(type) // allow empty type to always match and bypass query
		};

		if(!match)
			column(byte_view<string_view>(event_idx), std::nothrow, [&match, &type]
			(const string_view &value)
			{
				match = value == type;
			});

		if(match)
			if(!closure(event_idx))
				return false;
	}

	return true;
}

//
// room::events
//

ircd::m::room::events::events(const m::room &room,
                              const event::fetch::opts *const &fopts)
:room{room}
,_event
{
	fopts?
		*fopts:
	room.fopts?
		*room.fopts:
		event::fetch::default_opts
}
{
	assert(room.room_id);

	if(room.event_id)
		seek(room.event_id);
	else
		seek();
}

ircd::m::room::events::events(const m::room &room,
                              const event::id &event_id,
                              const event::fetch::opts *const &fopts)
:room{room}
,_event
{
	fopts?
		*fopts:
	room.fopts?
		*room.fopts:
		event::fetch::default_opts
}
{
	assert(room.room_id);

	seek(event_id);
}

ircd::m::room::events::events(const m::room &room,
                              const uint64_t &depth,
                              const event::fetch::opts *const &fopts)
:room{room}
,_event
{
	fopts?
		*fopts:
	room.fopts?
		*room.fopts:
		event::fetch::default_opts
}
{
	assert(room.room_id);

	// As a special convenience for the ctor only, if the depth=0 and
	// nothing is found another attempt is made for depth=1 for synapse
	// rooms which start at depth=1.
	if(!seek(depth) && depth == 0)
		seek(1);
}

bool
ircd::m::room::events::prefetch()
{
	assert(_event.fopts);
	return m::prefetch(event_idx(), *_event.fopts);
}

bool
ircd::m::room::events::prefetch(const string_view &event_prop)
{
	return m::prefetch(event_idx(), event_prop);
}

const ircd::m::event &
ircd::m::room::events::fetch()
{
	m::seek(_event, event_idx());
	return _event;
}

const ircd::m::event &
ircd::m::room::events::fetch(std::nothrow_t)
{
	m::seek(_event, event_idx(), std::nothrow);
	return _event;
}

const ircd::m::event &
ircd::m::room::events::operator*()
{
	return fetch(std::nothrow);
};

bool
ircd::m::room::events::preseek(const uint64_t &depth)
{
	char buf[dbs::ROOM_EVENTS_KEY_MAX_SIZE];
	const string_view key
	{
		depth != uint64_t(-1)?
			dbs::room_events_key(buf, room.room_id, depth):
			room.room_id
	};

	return db::prefetch(dbs::room_events, key);
}

bool
ircd::m::room::events::seek(const event::id &event_id)
{
	const event::idx &event_idx
	{
		m::index(event_id, std::nothrow)
	};

	return event_idx?
		seek_idx(event_idx):
		false;
}

bool
ircd::m::room::events::seek(const uint64_t &depth)
{
	char buf[dbs::ROOM_EVENTS_KEY_MAX_SIZE];
	const string_view seek_key
	{
		depth != uint64_t(-1)?
			dbs::room_events_key(buf, room.room_id, depth):
			room.room_id
	};

	this->it = dbs::room_events.begin(seek_key);
	return bool(*this);
}

bool
ircd::m::room::events::seek_idx(const event::idx &event_idx)
try
{
	uint64_t depth(0);
	if(event_idx)
		m::get(event_idx, "depth", mutable_buffer
		{
			reinterpret_cast<char *>(&depth), sizeof(depth)
		});

	char buf[dbs::ROOM_EVENTS_KEY_MAX_SIZE];
	const auto &seek_key
	{
		dbs::room_events_key(buf, room.room_id, depth, event_idx)
	};

	this->it = dbs::room_events.begin(seek_key);
	if(!bool(*this))
		return false;

	// Check if this event_idx is actually in this room
	if(event_idx && event_idx != this->event_idx())
		return false;

	return true;
}
catch(const db::not_found &e)
{
	return false;
}

ircd::m::room::events::operator
ircd::m::event::idx()
const
{
	return event_idx();
}

ircd::m::event::idx
ircd::m::room::events::event_idx()
const
{
	assert(bool(*this));
	const auto part
	{
		dbs::room_events_key(it->first)
	};

	return std::get<1>(part);
}

uint64_t
ircd::m::room::events::depth()
const
{
	assert(bool(*this));
	const auto part
	{
		dbs::room_events_key(it->first)
	};

	return std::get<0>(part);
}

//
// room::state
//

decltype(ircd::m::room::state::enable_history)
ircd::m::room::state::enable_history
{
	{ "name",     "ircd.m.room.state.enable_history" },
	{ "default",  true                               },
};

decltype(ircd::m::room::state::readahead_size)
ircd::m::room::state::readahead_size
{
	{ "name",     "ircd.m.room.state.readahead_size" },
	{ "default",  0L                                 },
};

//
// room::state::state
//

ircd::m::room::state::state(const m::room &room,
                            const event::fetch::opts *const &fopts)
:room_id
{
	room.room_id
}
,event_id
{
	room.event_id?
		event::id::buf{room.event_id}:
		event::id::buf{}
}
,fopts
{
	fopts?
		fopts:
		room.fopts
}
{
}

bool
ircd::m::room::state::prefetch(const string_view &type)
const
{
	return prefetch(type, string_view{});
}

bool
ircd::m::room::state::prefetch(const string_view &type,
                               const string_view &state_key)
const
{
	if(!present())
	{
		const history history
		{
			room_id, event_id
		};

		return history.prefetch(type, state_key);
	}

	char buf[dbs::ROOM_STATE_KEY_MAX_SIZE];
	const auto &key
	{
		dbs::room_state_key(buf, room_id, type, state_key)
	};

	return db::prefetch(dbs::room_state, key);
}

ircd::m::event::idx
ircd::m::room::state::get(const string_view &type,
                          const string_view &state_key)
const
{
	event::idx ret;
	get(type, state_key, event::closure_idx{[&ret]
	(const event::idx &event_idx)
	{
		ret = event_idx;
	}});

	return ret;
}

ircd::m::event::idx
ircd::m::room::state::get(std::nothrow_t,
                          const string_view &type,
                          const string_view &state_key)
const
{
	event::idx ret{0};
	get(std::nothrow, type, state_key, event::closure_idx{[&ret]
	(const event::idx &event_idx)
	{
		ret = event_idx;
	}});

	return ret;
}

void
ircd::m::room::state::get(const string_view &type,
                          const string_view &state_key,
                          const event::closure &closure)
const
{
	get(type, state_key, event::closure_idx{[this, &closure]
	(const event::idx &event_idx)
	{
		const event::fetch event
		{
			event_idx, fopts? *fopts : event::fetch::default_opts
		};

		closure(event);
	}});
}

void
ircd::m::room::state::get(const string_view &type,
                          const string_view &state_key,
                          const event::id::closure &closure)
const
{
	get(type, state_key, event::closure_idx{[&]
	(const event::idx &idx)
	{
		if(!m::event_id(idx, std::nothrow, closure))
			throw m::NOT_FOUND
			{
				"(%s,%s) in %s idx:%lu event_id :not found",
				type,
				state_key,
				string_view{room_id},
				idx,
			};
	}});
}

void
ircd::m::room::state::get(const string_view &type,
                          const string_view &state_key,
                          const event::closure_idx &closure)
const try
{
	if(!present())
	{
		const history history
		{
			room_id, event_id
		};

		closure(history.get(type, state_key));
		return;
	}

	auto &column{dbs::room_state};
	char key[dbs::ROOM_STATE_KEY_MAX_SIZE];
	column(dbs::room_state_key(key, room_id, type, state_key), [&closure]
	(const string_view &value)
	{
		closure(byte_view<event::idx>(value));
	});
}
catch(const db::not_found &e)
{
	throw m::NOT_FOUND
	{
		"(%s,%s) in %s :%s",
		type,
		state_key,
		string_view{room_id},
		e.what()
	};
}

bool
ircd::m::room::state::get(std::nothrow_t,
                          const string_view &type,
                          const string_view &state_key,
                          const event::closure &closure)
const
{
	return get(std::nothrow, type, state_key, event::closure_idx{[this, &closure]
	(const event::idx &event_idx)
	{
		const event::fetch event
		{
			event_idx, std::nothrow, fopts? *fopts : event::fetch::default_opts
		};

		closure(event);
	}});
}

bool
ircd::m::room::state::get(std::nothrow_t,
                          const string_view &type,
                          const string_view &state_key,
                          const event::id::closure &closure)
const
{
	return get(std::nothrow, type, state_key, event::closure_idx{[&closure]
	(const event::idx &idx)
	{
		m::event_id(idx, std::nothrow, closure);
	}});
}

bool
ircd::m::room::state::get(std::nothrow_t,
                          const string_view &type,
                          const string_view &state_key,
                          const event::closure_idx &closure)
const
{
	if(!present())
	{
		const history history
		{
			room_id, event_id
		};

		const auto event_idx
		{
			history.get(std::nothrow, type, state_key)
		};

		if(event_idx)
		{
			closure(event_idx);
			return true;
		}
		else return false;
	}

	auto &column{dbs::room_state};
	char key[dbs::ROOM_STATE_KEY_MAX_SIZE];
	return column(dbs::room_state_key(key, room_id, type, state_key), std::nothrow, [&closure]
	(const string_view &value)
	{
		closure(byte_view<event::idx>(value));
	});
}

bool
ircd::m::room::state::has(const event::idx &event_idx)
const
{
	static const event::fetch::opts fopts
	{
		event::keys::include { "type", "state_key" },
	};

	const m::event::fetch event
	{
		event_idx, std::nothrow, fopts
	};

	if(!event.valid)
		return false;

	const auto state_idx
	{
		get(std::nothrow, at<"type"_>(event), at<"state_key"_>(event))
	};

	assert(event_idx);
	return event_idx == state_idx;
}

bool
ircd::m::room::state::has(const string_view &type)
const
{
	return for_each(type, event::id::closure_bool{[](const m::event::id &)
	{
		return true;
	}});
}

bool
ircd::m::room::state::has(const string_view &type,
                          const string_view &state_key)
const
{
	if(!present())
	{
		const history history
		{
			room_id, event_id
		};

		return history.has(type, state_key);
	}

	auto &column{dbs::room_state};
	char key[dbs::ROOM_STATE_KEY_MAX_SIZE];
	return db::has(column, dbs::room_state_key(key, room_id, type, state_key));
}

size_t
ircd::m::room::state::count()
const
{
	if(!present())
		return count(string_view{});

	const db::gopts &opts
	{
		this->fopts? this->fopts->gopts : db::gopts{}
	};

	size_t ret(0);
	auto &column{dbs::room_state};
	for(auto it{column.begin(room_id, opts)}; bool(it); ++it)
		++ret;

	return ret;
}

size_t
ircd::m::room::state::count(const string_view &type)
const
{
	if(!present())
		return count(type);

	const db::gopts &opts
	{
		this->fopts? this->fopts->gopts : db::gopts{}
	};

	size_t ret(0);
	auto &column{dbs::room_state};
	for(auto it{column.begin(room_id, opts)}; bool(it); ++it)
	{
		const auto key(dbs::room_state_key(it->first));
		ret += std::get<0>(key) == type;
	}

	return ret;
}

void
ircd::m::room::state::for_each(const event::closure &closure)
const
{
	for_each(event::closure_bool{[&closure]
	(const m::event &event)
	{
		closure(event);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const event::closure_bool &closure)
const
{
	event::fetch event
	{
		fopts? *fopts : event::fetch::default_opts
	};

	return for_each(event::closure_idx_bool{[&event, &closure]
	(const event::idx &event_idx)
	{
		if(seek(event, event_idx, std::nothrow))
			if(!closure(event))
				return false;

		return true;
	}});
}

void
ircd::m::room::state::for_each(const event::id::closure &closure)
const
{
	for_each(event::id::closure_bool{[&closure]
	(const event::id &event_id)
	{
		closure(event_id);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const event::id::closure_bool &closure)
const
{
	return for_each(event::closure_idx_bool{[&closure]
	(const event::idx &idx)
	{
		bool ret{true};
		m::event_id(idx, std::nothrow, [&ret, &closure]
		(const event::id &id)
		{
			ret = closure(id);
		});

		return ret;
	}});
}

void
ircd::m::room::state::for_each(const event::closure_idx &closure)
const
{
	for_each(event::closure_idx_bool{[&closure]
	(const event::idx &event_idx)
	{
		closure(event_idx);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const event::closure_idx_bool &closure)
const
{
	return for_each(closure_bool{[&closure]
	(const string_view &type, const string_view &state_key, const event::idx &event_idx)
	{
		return closure(event_idx);
	}});
}

bool
ircd::m::room::state::for_each(const closure_bool &closure)
const
{
	if(!present())
	{
		const history history
		{
			room_id, event_id
		};

		return history.for_each([&closure]
		(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx)
		{
			return closure(type, state_key, event_idx);
		});
	}

	db::gopts opts
	{
		this->fopts? this->fopts->gopts : db::gopts{}
	};

	if(!opts.readahead)
		opts.readahead = size_t(readahead_size);

	auto &column{dbs::room_state};
	for(auto it{column.begin(room_id, opts)}; bool(it); ++it)
	{
		const byte_view<event::idx> idx(it->second);
		const auto key(dbs::room_state_key(it->first));
		if(!closure(std::get<0>(key), std::get<1>(key), idx))
			return false;
	}

	return true;
}

bool
ircd::m::room::state::for_each(const type_prefix &prefix,
                               const closure_bool &closure)
const
{
	bool ret(true), cont(true);
	for_each(closure_bool{[&prefix, &closure, &ret, &cont]
	(const string_view &type, const string_view &state_key, const event::idx &event_idx)
	{
		if(!startswith(type, string_view(prefix)))
			return cont;

		cont = false;
		ret = closure(type, state_key, event_idx);
		return ret;
	}});

	return ret;
}

void
ircd::m::room::state::for_each(const string_view &type,
                               const event::closure &closure)
const
{
	for_each(type, event::closure_bool{[&closure]
	(const m::event &event)
	{
		closure(event);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const event::closure_bool &closure)
const
{
	return type?
		for_each(type, string_view{}, closure):
		for_each(closure);
}

void
ircd::m::room::state::for_each(const string_view &type,
                               const event::id::closure &closure)
const
{
	for_each(type, event::id::closure_bool{[&closure]
	(const event::id &event_id)
	{
		closure(event_id);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const event::id::closure_bool &closure)
const
{
	return type?
		for_each(type, string_view{}, closure):
		for_each(closure);
}

void
ircd::m::room::state::for_each(const string_view &type,
                               const event::closure_idx &closure)
const
{
	for_each(type, event::closure_idx_bool{[&closure]
	(const event::idx &event_idx)
	{
		closure(event_idx);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const event::closure_idx_bool &closure)
const
{
	return type?
		for_each(type, string_view{}, closure):
		for_each(closure);
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const closure_bool &closure)
const
{
	return type?
		for_each(type, string_view{}, closure):
		for_each(closure);
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const string_view &state_key_lb,
                               const event::closure_bool &closure)
const
{
	event::fetch event
	{
		fopts? *fopts : event::fetch::default_opts
	};

	return for_each(type, state_key_lb, event::closure_idx_bool{[&event, &closure]
	(const event::idx &event_idx)
	{
		if(seek(event, event_idx, std::nothrow))
			if(!closure(event))
				return false;

		return true;
	}});
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const string_view &state_key_lb,
                               const event::id::closure_bool &closure)
const
{
	return for_each(type, state_key_lb, event::closure_idx_bool{[&closure]
	(const event::idx &idx)
	{
		bool ret{true};
		m::event_id(idx, std::nothrow, [&ret, &closure]
		(const event::id &id)
		{
			ret = closure(id);
		});

		return ret;
	}});
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const string_view &state_key_lb,
                               const event::closure_idx_bool &closure)
const
{
	return for_each(type, state_key_lb, closure_bool{[&closure]
	(const string_view &type, const string_view &state_key, const event::idx &event_idx)
	{
		return closure(event_idx);
	}});
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const string_view &state_key_lb,
                               const closure_bool &closure)
const
{
	if(!present())
	{
		const history history
		{
			room_id, event_id
		};

		return history.for_each(type, state_key_lb, [&closure]
		(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx)
		{
			return closure(type, state_key, event_idx);
		});
	}

	char keybuf[dbs::ROOM_STATE_KEY_MAX_SIZE];
	const auto &key
	{
		dbs::room_state_key(keybuf, room_id, type, state_key_lb)
	};

	db::gopts opts
	{
		this->fopts? this->fopts->gopts : db::gopts{}
	};

	if(!opts.readahead)
		opts.readahead = size_t(readahead_size);

	auto &column{dbs::room_state};
	for(auto it{column.begin(key, opts)}; bool(it); ++it)
	{
		const auto key
		{
			dbs::room_state_key(it->first)
		};

		if(std::get<0>(key) != type)
			break;

		const byte_view<event::idx> idx(it->second);
		if(!closure(std::get<0>(key), std::get<1>(key), idx))
			return false;
	}

	return true;
}

/// Figure out if this instance of room::state is presenting the current
/// "present" state of the room or the state of the room at some previous
/// event. This is an important distinction because the present state of
/// the room should provide optimal performance for the functions of this
/// interface by using the present state table. Prior states will use the
/// state btree.
bool
ircd::m::room::state::present()
const
{
	// When no event_id is passed to the state constructor that immediately
	// indicates the present state of the room is sought.
	if(!event_id)
		return true;

	// When the global configuration disables history, always consider the
	// present state. (disabling may yield unexpected incorrect results by
	// returning the present state without error).
	if(!enable_history)
		return true;

	// Check the cached value from a previous false result of this function
	// before doing any real work/IO below. If this function ever returned
	// false it will never return true after.
	if(_not_present)
		return false;

	const auto head_id
	{
		m::head(std::nothrow, room_id)
	};

	// If the event_id passed is exactly the latest event we can obviously
	// consider this the present state.
	if(!head_id || head_id == event_id)
		return true;

	// This result is cacheable because once it's no longer the present
	// it will never be again. Panta chorei kai ouden menei. Note that this
	// is a const member function; the cache variable is an appropriate case
	// for the 'mutable' keyword.
	_not_present = true;
	return false;
}

//
// room::state::history
//

ircd::m::room::state::history::history(const m::room &room)
:history
{
	room, -1
}
{
}

ircd::m::room::state::history::history(const m::room::id &room_id,
                                       const m::event::id &event_id)
:history
{
	m::room
	{
		room_id, event_id
	}
}
{
}

ircd::m::room::state::history::history(const m::room &room,
                                       const int64_t &bound)
:space
{
	room
}
,bound
{
	bound < 0 && room.event_id?
		m::get<int64_t>(m::index(room.event_id), "depth"):
		bound
}
{
}

bool
ircd::m::room::state::history::prefetch(const string_view &type)
const
{
	return prefetch(type, string_view{});
}

bool
ircd::m::room::state::history::prefetch(const string_view &type,
                                        const string_view &state_key)
const
{
	return space.prefetch(type, state_key, bound);
}

ircd::m::event::idx
ircd::m::room::state::history::get(const string_view &type,
                                   const string_view &state_key)
const
{
	const auto ret
	{
		get(std::nothrow, type, state_key)
	};

	if(unlikely(!ret))
		throw m::NOT_FOUND
		{
			"(%s,%s) in %s @%ld$%s",
			type,
			state_key,
			string_view{space.room.room_id},
			bound,
			string_view{space.room.event_id},
		};

	return ret;
}

ircd::m::event::idx
ircd::m::room::state::history::get(std::nothrow_t,
                                   const string_view &type,
                                   const string_view &state_key)
const
{
	event::idx ret{0};
	assert(type && defined(state_key));
	for_each(type, state_key, [&ret]
	(const auto &, const auto &, const auto &, const auto &event_idx)
	{
		ret = event_idx;
		return false;
	});

	return ret;
}

bool
ircd::m::room::state::history::has(const string_view &type)
const
{
	return has(type, string_view{});
}

bool
ircd::m::room::state::history::has(const string_view &type,
                                   const string_view &state_key)
const
{
	return !for_each(type, state_key, []
	(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx)
	{
		return false;
	});
}

size_t
ircd::m::room::state::history::count(const string_view &type)
const
{
	return count(type, string_view{});
}

size_t
ircd::m::room::state::history::count(const string_view &type,
                                     const string_view &state_key)
const
{
	size_t ret(0);
	for_each(type, state_key, [&ret]
	(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::room::state::history::for_each(const closure &closure)
const
{
	return for_each(string_view{}, string_view{}, closure);
}

bool
ircd::m::room::state::history::for_each(const string_view &type,
                                        const closure &closure)
const
{
	return for_each(type, string_view{}, closure);
}

bool
ircd::m::room::state::history::for_each(const string_view &type,
                                        const string_view &state_key,
                                        const closure &closure)
const
{
	char type_buf[m::event::TYPE_MAX_SIZE];
	char state_key_buf[m::event::STATE_KEY_MAX_SIZE];

	string_view last_type;
	string_view last_state_key;

	return space.for_each(type, state_key, [&]
	(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx)
	{
		if(bound > -1 && depth >= bound)
			return true;

		if(type == last_type && state_key == last_state_key)
			return true;

		if(!closure(type, state_key, depth, event_idx))
			return false;

		if(type != last_type)
			last_type = { type_buf, copy(type_buf, type) };

		if(state_key != last_state_key)
			last_state_key = { state_key_buf, copy(state_key_buf, state_key) };

		return true;
	});
}

//
// room::state::space
//

ircd::m::room::state::space::space(const m::room &room)
:room
{
	room
}
{
}

bool
ircd::m::room::state::space::prefetch(const string_view &type)
const
{
	return prefetch(type, string_view{});
}

bool
ircd::m::room::state::space::prefetch(const string_view &type,
                                      const string_view &state_key)
const
{
	return prefetch(type, state_key, -1);
}

bool
ircd::m::room::state::space::prefetch(const string_view &type,
                                      const string_view &state_key,
                                      const int64_t &depth)
const
{
	const int64_t &_depth
	{
		type? depth : 0L
	};

	char buf[dbs::ROOM_STATE_SPACE_KEY_MAX_SIZE];
	const string_view &key
	{
		dbs::room_state_space_key(buf, room.room_id, type, state_key, _depth, 0UL)
	};

	return db::prefetch(dbs::room_state_space, key);
}

bool
ircd::m::room::state::space::has(const string_view &type)
const
{
	return has(type, string_view{});
}

bool
ircd::m::room::state::space::has(const string_view &type,
                                 const string_view &state_key)
const
{
	return has(type, state_key, -1);
}

bool
ircd::m::room::state::space::has(const string_view &type,
                                 const string_view &state_key,
                                 const int64_t &depth)
const
{
	return !for_each(type, state_key, depth, []
	(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx)
	{
		return false;
	});
}

size_t
ircd::m::room::state::space::count()
const
{
	return count(string_view{});
}

size_t
ircd::m::room::state::space::count(const string_view &type)
const
{
	return count(type, string_view{});
}

size_t
ircd::m::room::state::space::count(const string_view &type,
                                   const string_view &state_key)
const
{
	return count(type, state_key, -1L);
}

size_t
ircd::m::room::state::space::count(const string_view &type,
                                   const string_view &state_key,
                                   const int64_t &depth)
const
{
	size_t ret(0);
	for_each(type, state_key, depth, [&ret]
	(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::room::state::space::for_each(const closure &closure)
const
{
	return for_each(string_view{}, string_view{}, -1L, closure);
}

bool
ircd::m::room::state::space::for_each(const string_view &type,
                                      const closure &closure)
const
{
	return for_each(type, string_view{}, -1L, closure);
}

bool
ircd::m::room::state::space::for_each(const string_view &type,
                                      const string_view &state_key,
                                      const closure &closure)
const
{
	return for_each(type, state_key, -1L, closure);
}

bool
ircd::m::room::state::space::for_each(const string_view &type,
                                      const string_view &state_key,
                                      const int64_t &depth,
                                      const closure &closure)
const
{
	const int64_t &_depth
	{
		type? depth : 0L
	};

	char buf[dbs::ROOM_STATE_SPACE_KEY_MAX_SIZE];
	const string_view &key
	{
		dbs::room_state_space_key(buf, room.room_id, type, state_key, _depth, 0UL)
	};

	auto it
	{
		dbs::room_state_space.begin(key)
	};

	for(; it; ++it)
	{
		const auto &[_type, _state_key, _depth, _event_idx]
		{
			dbs::room_state_space_key(it->first)
		};

		if(type && type != _type)
			break;

		if(state_key && state_key != _state_key)
			break;

		if(depth >= 0 && depth != _depth)
			break;

		if(!closure(_type, _state_key, _depth, _event_idx))
			return false;
	}

	return true;
}

//
// room::state::space::rebuild
//

ircd::m::room::state::space::rebuild::rebuild(const room::id &room_id)
{
	db::txn txn
	{
		*m::dbs::events
	};

	m::room::events it
	{
		room_id, uint64_t(0)
	};

	if(!it)
		return;

	const bool check_auth
	{
		!m::internal(room_id)
	};

	size_t state_count(0), messages_count(0), state_deleted(0);
	for(; it; ++it, ++messages_count) try
	{
		const m::event::idx &event_idx
		{
			it.event_idx()
		};

		if(!state::is(std::nothrow, event_idx))
			continue;

		++state_count;
		const m::event &event{*it};
		const auto &[pass_static, reason_static]
		{
			check_auth?
				room::auth::check_static(event):
				room::auth::passfail{true, {}}
		};

		if(!pass_static)
			log::dwarning
			{
				log, "%s in %s erased from state space (static) :%s",
				string_view{event.event_id},
				string_view{room_id},
				what(reason_static),
			};

		const auto &[pass_relative, reason_relative]
		{
			!check_auth?
				room::auth::passfail{true, {}}:
			pass_static?
				room::auth::check_relative(event):
				room::auth::passfail{false, {}},
		};

		if(pass_static && !pass_relative)
			log::dwarning
			{
				log, "%s in %s erased from state space (relative) :%s",
				string_view{event.event_id},
				string_view{room_id},
				what(reason_relative),
			};

		dbs::write_opts opts;
		opts.event_idx = event_idx;

		opts.appendix.reset();
		opts.appendix.set(dbs::appendix::ROOM_STATE_SPACE);

		opts.op = pass_static && pass_relative? db::op::SET : db::op::DELETE;
		state_deleted += opts.op == db::op::DELETE;

		dbs::write(txn, event, opts);
	}
	catch(const ctx::interrupted &e)
	{
		log::dwarning
		{
			log, "room::state::space::rebuild :%s",
			e.what()
		};

		throw;
	}
	catch(const std::exception &e)
	{
		log::error
		{
			log, "room::state::space::rebuild :%s",
			e.what()
		};
	}

	log::info
	{
		log, "room::state::space::rebuild %s complete msgs:%zu state:%zu del:%zu transaction elems:%zu size:%s",
		string_view{room_id},
		messages_count,
		state_count,
		state_deleted,
		txn.size(),
		pretty(iec(txn.bytes()))
	};

	txn();
}

//
// room::members
//

bool
ircd::m::room::members::empty()
const
{
	return empty(string_view{}, string_view{});
}

bool
ircd::m::room::members::empty(const string_view &membership)
const
{
	return empty(membership, string_view{});
}

bool
ircd::m::room::members::empty(const string_view &membership,
                              const string_view &host)
const
{
	return for_each(membership, host, closure{[]
	(const user::id &user_id)
	{
		return false;
	}});
}

size_t
ircd::m::room::members::count()
const
{
	return count(string_view{}, string_view{});
}

size_t
ircd::m::room::members::count(const string_view &membership)
const
{
	return count(membership, string_view{});
}

size_t
ircd::m::room::members::count(const string_view &membership,
                              const string_view &host)
const
{
	size_t ret{0};
	for_each(membership, host, closure{[&ret]
	(const user::id &user_id)
	{
		++ret;
		return true;
	}});

	return ret;
}

bool
ircd::m::room::members::for_each(const closure &closure)
const
{
	return for_each(string_view{}, closure);
}

bool
ircd::m::room::members::for_each(const closure_idx &closure)
const
{
	return for_each(string_view{}, closure);
}

bool
ircd::m::room::members::for_each(const string_view &membership,
                                 const closure &closure)
const
{
	return for_each(membership, string_view{}, closure);
}

bool
ircd::m::room::members::for_each(const string_view &membership,
                                 const closure_idx &closure)
const
{
	return for_each(membership, string_view{}, closure);
}

bool
ircd::m::room::members::for_each(const string_view &membership,
                                 const string_view &host,
                                 const closure &closure)
const
{
	const m::room::state state
	{
		room
	};

	const bool present
	{
		state.present()
	};

	// joined members optimization. Only possible when seeking
	// membership="join" on the present state of the room.
	if(membership == "join" && present)
		return for_each_join_present(host, closure);

	return this->for_each(membership, host, [&closure]
	(const auto &user_id, const auto &event_idx)
	{
		return closure(user_id);
	});
}

bool
ircd::m::room::members::for_each(const string_view &membership,
                                 const string_view &host,
                                 const closure_idx &closure)
const
{
	const m::room::state state
	{
		room
	};

	const bool present
	{
		state.present()
	};

	// joined members optimization. Only possible when seeking
	// membership="join" on the present state of the room.
	if(membership == "join" && present)
		return for_each_join_present(host, [&closure, &state]
		(const id::user &user_id)
		{
			const auto &event_idx
			{
				state.get(std::nothrow, "m.room.member", user_id)
			};

			if(unlikely(!event_idx))
			{
				log::error
				{
					log, "Failed member:%s event_idx:%lu in room_joined of %s",
					string_view{user_id},
					event_idx,
					string_view{state.room_id},
				};

				return true;
			}

			if(!closure(user_id, event_idx))
				return false;

			return true;
		});

	return state.for_each("m.room.member", [this, &host, &membership, &closure]
	(const string_view &type, const string_view &state_key, const event::idx &event_idx)
	{
		const m::user::id &user_id
		{
			state_key
		};

		if(host && user_id.host() != host)
			return true;

		return !membership || m::membership(event_idx, membership)?
			closure(user_id, event_idx):
			true;
	});
}

bool
ircd::m::room::members::for_each_join_present(const string_view &host,
                                              const closure &closure)
const
{
	db::domain &index
	{
		dbs::room_joined
	};

	char keybuf[dbs::ROOM_JOINED_KEY_MAX_SIZE];
	const string_view &key
	{
		dbs::room_joined_key(keybuf, room.room_id, host)
	};

	auto it
	{
		index.begin(key)
	};

	for(; bool(it); ++it)
	{
		const auto &[origin, user_id]
		{
			dbs::room_joined_key(it->first)
		};

		if(host && origin != host)
			break;

		if(!closure(user_id))
			return false;
	}

	return true;
}

//
// room::origins
//

ircd::string_view
ircd::m::room::origins::random(const mutable_buffer &buf,
                               const closure_bool &proffer)
const
{
	string_view ret;
	const auto closure{[&buf, &proffer, &ret]
	(const string_view &origin)
	{
		ret = { data(buf), copy(buf, origin) };
	}};

	random(closure, proffer);
	return ret;
}

bool
ircd::m::room::origins::random(const closure &view,
                               const closure_bool &proffer)
const
{
	return random(*this, view, proffer);
}

bool
ircd::m::room::origins::random(const origins &origins,
                               const closure &view,
                               const closure_bool &proffer)
{
	bool ret{false};
	const size_t max
	{
		origins.count()
	};

	if(unlikely(!max))
		return ret;

	auto select
	{
		ssize_t(rand::integer(0, max - 1))
	};

	const closure_bool closure{[&proffer, &view, &select]
	(const string_view &origin)
	{
		if(select-- > 0)
			return true;

		// Test if this random selection is "ok" e.g. the callback allows the
		// user to test a blacklist for this origin. Skip to next if not.
		if(proffer && !proffer(origin))
		{
			++select;
			return true;
		}

		view(origin);
		return false;
	}};

	const auto iteration{[&origins, &closure, &ret]
	{
		ret = !origins.for_each(closure);
	}};

	// Attempt select on first iteration
	iteration();

	// If nothing was OK between the random int and the end of the iteration
	// then start again and pick the first OK.
	if(!ret && select >= 0)
		iteration();

	return ret;
}

bool
ircd::m::room::origins::empty()
const
{
	return for_each(closure_bool{[]
	(const string_view &)
	{
		// return false to break and return false.
		return false;
	}});
}

size_t
ircd::m::room::origins::count()
const
{
	size_t ret{0};
	for_each([&ret](const string_view &)
	{
		++ret;
	});

	return ret;
}

size_t
ircd::m::room::origins::count_error()
const
{
	size_t ret{0};
	for_each([&ret](const string_view &server)
	{
		ret += !ircd::empty(server::errmsg(server));
	});

	return ret;
}

size_t
ircd::m::room::origins::count_online()
const
{
	ssize_t ret
	{
		0 - ssize_t(count_error())
	};

	for_each([&ret](const string_view &hostport)
	{
		ret += bool(server::exists(hostport));
	});

	assert(ret >= 0L);
	return std::max(ret, 0L);
}

/// Tests if argument is the only origin in the room.
/// If a zero or more than one origins exist, returns false. If the only origin
/// in the room is the argument origin, returns true.
bool
ircd::m::room::origins::only(const string_view &origin)
const
{
	ushort ret{2};
	for_each(closure_bool{[&ret, &origin]
	(const string_view &origin_) -> bool
	{
		if(origin == origin_)
			ret = 1;
		else
			ret = 0;

		return ret;
	}});

	return ret == 1;
}

bool
ircd::m::room::origins::has(const string_view &origin)
const
{
	db::domain &index
	{
		dbs::room_joined
	};

	char querybuf[dbs::ROOM_JOINED_KEY_MAX_SIZE];
	const auto query
	{
		dbs::room_joined_key(querybuf, room.room_id, origin)
	};

	auto it
	{
		index.begin(query)
	};

	if(!it)
		return false;

	const string_view &key
	{
		lstrip(it->first, "\0"_sv)
	};

	const string_view &key_origin
	{
		std::get<0>(dbs::room_joined_key(key))
	};

	return key_origin == origin;
}

void
ircd::m::room::origins::for_each(const closure &view)
const
{
	for_each(closure_bool{[&view]
	(const string_view &origin)
	{
		view(origin);
		return true;
	}});
}

bool
ircd::m::room::origins::for_each(const closure_bool &view)
const
{
	string_view last;
	char lastbuf[rfc1035::NAME_BUFSIZE];
	return _for_each(*this, [&last, &lastbuf, &view]
	(const string_view &key)
	{
		const string_view &origin
		{
			std::get<0>(dbs::room_joined_key(key))
		};

		if(origin == last)
			return true;

		if(!view(origin))
			return false;

		last = { lastbuf, copy(lastbuf, origin) };
		return true;
	});
}

bool
ircd::m::room::origins::_for_each(const origins &origins,
                                  const closure_bool &view)
{
	db::domain &index
	{
		dbs::room_joined
	};

	auto it
	{
		index.begin(origins.room.room_id)
	};

	for(; bool(it); ++it)
	{
		const string_view &key
		{
			lstrip(it->first, "\0"_sv)
		};

		if(!view(key))
			return false;
	}

	return true;
}

//
// room::aliases
//

size_t
ircd::m::room::aliases::count()
const
{
	return count(string_view{});
}

size_t
ircd::m::room::aliases::count(const string_view &server)
const
{
	size_t ret(0);
	for_each(server, [&ret](const auto &a)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::room::aliases::has(const alias &alias)
const
{
	return !for_each(alias.host(), [&alias]
	(const id::room_alias &a)
	{
		assert(a.host() == alias.host());
		return a == alias? false : true; // false to break on found
	});
}

bool
ircd::m::room::aliases::for_each(const closure_bool &closure)
const
{
	const room::state state
	{
		room
	};

	return state.for_each("m.room.aliases", [this, &closure]
	(const string_view &type, const string_view &state_key, const event::idx &)
	{
		return for_each(state_key, closure);
	});
}

bool
ircd::m::room::aliases::for_each(const string_view &server,
                                 const closure_bool &closure)
const
{
	if(!server)
		return for_each(closure);

	return for_each(room, server, closure);
}

bool
ircd::m::room::aliases::for_each(const m::room &room,
                                 const string_view &server,
                                 const closure_bool &closure)
{
	using prototype = bool (const m::room &, const string_view &, const closure_bool &);

	static mods::import<prototype> call
	{
		"m_room_aliases", "ircd::m::room::aliases::for_each"
	};

	return call(room, server, closure);
}

//
// room::aliases::cache
//

bool
ircd::m::room::aliases::cache::del(const alias &a)
{
	using prototype = bool (const alias &);

	static mods::import<prototype> call
	{
		"m_room_aliases", "ircd::m::room::aliases::cache::del"
	};

	return call(a);
}

bool
ircd::m::room::aliases::cache::set(const alias &a,
                                   const id &i)
{
	using prototype = bool (const alias &, const id &);

	static mods::import<prototype> call
	{
		"m_room_aliases", "ircd::m::room::aliases::cache::set"
	};

	return call(a, i);
}

bool
ircd::m::room::aliases::cache::fetch(std::nothrow_t,
                                     const alias &a,
                                     const net::hostport &hp)
try
{
	fetch(a, hp);
	return true;
}
catch(const std::exception &e)
{
	thread_local char buf[384];
	log::error
	{
		log, "Failed to fetch room_id for %s from %s :%s",
		string_view{a},
		string(buf, hp),
		e.what(),
	};

	return false;
}

void
ircd::m::room::aliases::cache::fetch(const alias &a,
                                     const net::hostport &hp)
{
	using prototype = void (const alias &, const net::hostport &);

	static mods::import<prototype> call
	{
		"m_room_aliases", "ircd::m::room::aliases::cache::fetch"
	};

	return call(a, hp);
}

ircd::m::room::id::buf
ircd::m::room::aliases::cache::get(const alias &a)
{
	id::buf ret;
	get(a, [&ret]
	(const id &room_id)
	{
		ret = room_id;
	});

	return ret;
}

ircd::m::room::id::buf
ircd::m::room::aliases::cache::get(std::nothrow_t,
                                   const alias &a)
{
	id::buf ret;
	get(std::nothrow, a, [&ret]
	(const id &room_id)
	{
		ret = room_id;
	});

	return ret;
}

void
ircd::m::room::aliases::cache::get(const alias &a,
                                   const id::closure &c)
{
	if(!get(std::nothrow, a, c))
		throw m::NOT_FOUND
		{
			"Cannot find room_id for %s",
			string_view{a}
		};
}

bool
ircd::m::room::aliases::cache::get(std::nothrow_t,
                                   const alias &a,
                                   const id::closure &c)
{
	using prototype = bool (std::nothrow_t, const alias &, const id::closure &);

	static mods::import<prototype> call
	{
		"m_room_aliases", "ircd::m::room::aliases::cache::get"
	};

	return call(std::nothrow, a, c);
}

bool
ircd::m::room::aliases::cache::has(const alias &a)
{
	using prototype = bool (const alias &);

	static mods::import<prototype> call
	{
		"m_room_aliases", "ircd::m::room::aliases::cache::has"
	};

	return call(a);
}

bool
ircd::m::room::aliases::cache::for_each(const closure_bool &c)
{
	return for_each(string_view{}, c);
}

bool
ircd::m::room::aliases::cache::for_each(const string_view &s,
                                        const closure_bool &c)
{
	using prototype = bool (const string_view &, const closure_bool &);

	static mods::import<prototype> call
	{
		"m_room_aliases", "ircd::m::room::aliases::cache::for_each"
	};

	return call(s, c);
}

//
// room::power
//

decltype(ircd::m::room::power::default_creator_level)
ircd::m::room::power::default_creator_level
{
	100
};

decltype(ircd::m::room::power::default_power_level)
ircd::m::room::power::default_power_level
{
	50
};

decltype(ircd::m::room::power::default_event_level)
ircd::m::room::power::default_event_level
{
	0
};

decltype(ircd::m::room::power::default_user_level)
ircd::m::room::power::default_user_level
{
	0
};

ircd::json::object
ircd::m::room::power::default_content(const mutable_buffer &buf,
                                      const m::user::id &creator)
{
	return compose_content(buf, [&creator]
	(const string_view &key, json::stack::object &object)
	{
		if(key != "users")
			return;

		assert(default_creator_level == 100);
		json::stack::member
		{
			object, creator, json::value(default_creator_level)
		};
	});
}

ircd::json::object
ircd::m::room::power::compose_content(const mutable_buffer &buf,
                                      const compose_closure &closure)
{
	json::stack out{buf};
	json::stack::object content{out};

	assert(default_power_level == 50);
	json::stack::member
	{
		content, "ban", json::value(default_power_level)
	};

	{
		json::stack::object events
		{
			content, "events"
		};

		closure("events", events);
	}

	assert(default_event_level == 0);
	json::stack::member
	{
		content, "events_default", json::value(default_event_level)
	};

	json::stack::member
	{
		content, "invite", json::value(default_power_level)
	};

	json::stack::member
	{
		content, "kick", json::value(default_power_level)
	};

	{
		json::stack::object notifications
		{
			content, "notifications"
		};

		json::stack::member
		{
			notifications, "room", json::value(default_power_level)
		};

		closure("notifications", notifications);
	}

	json::stack::member
	{
		content, "redact", json::value(default_power_level)
	};

	json::stack::member
	{
		content, "state_default", json::value(default_power_level)
	};

	{
		json::stack::object users
		{
			content, "users"
		};

		closure("users", users);
	}

	assert(default_user_level == 0);
	json::stack::member
	{
		content, "users_default", json::value(default_user_level)
	};

	content.~object();
	return json::object{out.completed()};
}

//
// room::power::power
//

ircd::m::room::power::power(const m::room &room)
:power
{
	room, room.get(std::nothrow, "m.room.power_levels", "")
}
{
}

ircd::m::room::power::power(const m::room &room,
                            const event::idx &power_event_idx)
:room
{
	room
}
,power_event_idx
{
	power_event_idx
}
{
}

ircd::m::room::power::power(const m::event &power_event,
                            const m::event &create_event)
:power
{
	power_event, m::user::id(unquote(json::get<"content"_>(create_event).get("creator")))
}
{
}

ircd::m::room::power::power(const m::event &power_event,
                            const m::user::id &room_creator_id)
:power
{
	json::get<"content"_>(power_event), room_creator_id
}
{
}

ircd::m::room::power::power(const json::object &power_event_content,
                            const m::user::id &room_creator_id)
:power_event_content
{
	power_event_content
}
,room_creator_id
{
	room_creator_id
}
{
}

/// "all who attain great power and riches make use of either force or fraud"
///
/// Returns bool for "allow" or "deny"
///
/// Provide the user invoking the power. The return value indicates whether
/// they have the power.
///
/// Provide the property/event_type. There are two usages here: 1. This is a
/// string corresponding to one of the spec top-level properties like "ban"
/// and "redact". In this case, the type and state_key parameters to this
/// function are not used. 2. This string is empty or "events" in which case
/// the type parameter is used to fetch the power threshold for that type.
/// For state events of a type, the state_key must be provided for inspection
/// here as well.
bool
ircd::m::room::power::operator()(const m::user::id &user_id,
                                 const string_view &prop,
                                 const string_view &type,
                                 const string_view &state_key)
const
{
	const auto &user_level
	{
		level_user(user_id)
	};

	const auto &required_level
	{
		empty(prop) || prop == "events"?
			level_event(type, state_key):
			level(prop)
	};

	return user_level >= required_level;
}

int64_t
ircd::m::room::power::level_user(const m::user::id &user_id)
const try
{
	int64_t ret
	{
		default_user_level
	};

	const auto closure{[&user_id, &ret]
	(const json::object &content)
	{
		const auto users_default
		{
			content.get<int64_t>("users_default", default_user_level)
		};

		const json::object &users
		{
			content.get("users")
		};

		ret = users.get<int64_t>(user_id, users_default);
	}};

	const bool has_power_levels_event
	{
		view(closure)
	};

	if(!has_power_levels_event)
	{
		if(room_creator_id && user_id == room_creator_id)
			ret = default_creator_level;

		if(room.room_id && creator(room, user_id))
			ret = default_creator_level;
	}

	return ret;
}
catch(const json::error &e)
{
	return default_user_level;
}

int64_t
ircd::m::room::power::level_event(const string_view &type)
const try
{
	int64_t ret
	{
		default_event_level
	};

	const auto closure{[&type, &ret]
	(const json::object &content)
	{
		const auto &events_default
		{
			content.get<int64_t>("events_default", default_event_level)
		};

		const json::object &events
		{
			content.get("events")
		};

		ret = events.get<int64_t>(type, events_default);
	}};

	const bool has_power_levels_event
	{
		view(closure)
	};

	return ret;
}
catch(const json::error &e)
{
	return default_event_level;
}

int64_t
ircd::m::room::power::level_event(const string_view &type,
                                  const string_view &state_key)
const try
{
	if(!defined(state_key))
		return level_event(type);

	int64_t ret
	{
		default_power_level
	};

	const auto closure{[&type, &ret]
	(const json::object &content)
	{
		const auto &state_default
		{
			content.get<int64_t>("state_default", default_power_level)
		};

		const json::object &events
		{
			content.get("events")
		};

		ret = events.get<int64_t>(type, state_default);
	}};

	const bool has_power_levels_event
	{
		view(closure)
	};

	return ret;
}
catch(const json::error &e)
{
	return default_power_level;
}

int64_t
ircd::m::room::power::level(const string_view &prop)
const try
{
	int64_t ret
	{
		default_power_level
	};

	view([&prop, &ret]
	(const json::object &content)
	{
		ret = content.at<int64_t>(prop);
	});

	return ret;
}
catch(const json::error &e)
{
	return default_power_level;
}

size_t
ircd::m::room::power::count_levels()
const
{
	size_t ret{0};
	for_each([&ret]
	(const string_view &, const int64_t &)
	{
		++ret;
	});

	return ret;
}

size_t
ircd::m::room::power::count_collections()
const
{
	size_t ret{0};
	view([&ret]
	(const json::object &content)
	{
		for(const auto &member : content)
			ret += json::type(member.second) == json::OBJECT;
	});

	return ret;
}

size_t
ircd::m::room::power::count(const string_view &prop)
const
{
	size_t ret{0};
	for_each(prop, [&ret]
	(const string_view &, const int64_t &)
	{
		++ret;
	});

	return ret;
}

bool
ircd::m::room::power::has_event(const string_view &type)
const try
{
	bool ret{false};
	view([&type, &ret]
	(const json::object &content)
	{
		const json::object &events
		{
			content.at("events")
		};

		const string_view &level
		{
			unquote(events.at(type))
		};

		ret = json::type(level) == json::NUMBER;
	});

	return ret;
}
catch(const json::error &)
{
	return false;
}

bool
ircd::m::room::power::has_user(const m::user::id &user_id)
const try
{
	bool ret{false};
	view([&user_id, &ret]
	(const json::object &content)
	{
		const json::object &users
		{
			content.at("users")
		};

		const string_view &level
		{
			unquote(users.at(user_id))
		};

		ret = json::type(level) == json::NUMBER;
	});

	return ret;
}
catch(const json::error &)
{
	return false;
}

bool
ircd::m::room::power::has_collection(const string_view &prop)
const
{
	bool ret{false};
	view([&prop, &ret]
	(const json::object &content)
	{
		const auto &value{content.get(prop)};
		if(value && json::type(value) == json::OBJECT)
			ret = true;
	});

	return ret;
}

bool
ircd::m::room::power::has_level(const string_view &prop)
const
{
	bool ret{false};
	view([&prop, &ret]
	(const json::object &content)
	{
		const auto &value(unquote(content.get(prop)));
		if(value && json::type(value) == json::NUMBER)
			ret = true;
	});

	return ret;
}

void
ircd::m::room::power::for_each(const closure &closure)
const
{
	for_each(string_view{}, closure);
}

bool
ircd::m::room::power::for_each(const closure_bool &closure)
const
{
	return for_each(string_view{}, closure);
}

void
ircd::m::room::power::for_each(const string_view &prop,
                               const closure &closure)
const
{
	for_each(prop, closure_bool{[&closure]
	(const string_view &key, const int64_t &level)
	{
		closure(key, level);
		return true;
	}});
}

bool
ircd::m::room::power::for_each(const string_view &prop,
                               const closure_bool &closure)
const
{
	bool ret{true};
	view([&prop, &closure, &ret]
	(const json::object &content)
	{
		const json::object &collection
		{
			// This little cmov gimmick sets collection to be the outer object
			// itself if no property was given, allowing us to reuse this func
			// for all iterations of key -> level mappings.
			prop? json::object{content.get(prop)} : content
		};

		const string_view _collection{collection};
		if(prop && (!_collection || json::type(_collection) != json::OBJECT))
			return;

		for(auto it(begin(collection)); it != end(collection) && ret; ++it)
		{
			const auto &member(*it);
			if(json::type(unquote(member.second)) != json::NUMBER)
				continue;

			const auto &key
			{
				unquote(member.first)
			};

			const auto &val
			{
				lex_cast<int64_t>(member.second)
			};

			ret = closure(key, val);
		}
	});

	return ret;
}

bool
ircd::m::room::power::view(const std::function<void (const json::object &)> &closure)
const
{
	if(power_event_idx)
		if(m::get(std::nothrow, power_event_idx, "content", closure))
			return true;

	closure(power_event_content);
	return !empty(power_event_content);
}

//
// room::stats
//

size_t
__attribute__((noreturn))
ircd::m::room::stats::bytes_total(const m::room &room)
{
	throw m::UNSUPPORTED
	{
		"Not yet implemented."
	};
}

size_t
__attribute__((noreturn))
ircd::m::room::stats::bytes_total_compressed(const m::room &room)
{
	throw m::UNSUPPORTED
	{
		"Not yet implemented."
	};
}

size_t
ircd::m::room::stats::bytes_json(const m::room &room)
{
	size_t ret(0);
	for(m::room::events it(room); it; --it)
	{
		const m::event::idx &event_idx
		{
			it.event_idx()
		};

		const byte_view<string_view> key
		{
			event_idx
		};

		static const db::gopts gopts
		{
			db::get::NO_CACHE
		};

		ret += db::bytes_value(m::dbs::event_json, key, gopts);
	}

	return ret;
}

size_t
__attribute__((noreturn))
ircd::m::room::stats::bytes_json_compressed(const m::room &room)
{
	throw m::UNSUPPORTED
	{
		"Not yet implemented."
	};
}
