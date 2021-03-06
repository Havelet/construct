// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "rooms.h"

namespace ircd::m
{
	static event::id::buf invite_foreign(const event &);
	static void on_invite_foreign(const event &, vm::eval &);

	extern hookfn<vm::eval &> invite_foreign_hook;
}

decltype(ircd::m::invite_foreign_hook)
ircd::m::invite_foreign_hook
{
	on_invite_foreign,
	{
		{ "_site",          "vm.issue"      },
		{ "type",           "m.room.member" },
		{ "membership",     "invite"        },
	}
};

ircd::resource::response
post__invite(ircd::client &client,
             const ircd::resource::request &request,
             const ircd::m::room::id &room_id)
{
	using namespace ircd;

	const m::user::id &target
	{
		unquote(request.at("user_id"))
	};

	const m::user::id &sender
	{
		request.user_id
	};

	const auto event_id
	{
		m::invite(room_id, target, sender)
	};

	return resource::response
	{
		client, http::OK
	};
}

ircd::m::event::id::buf
IRCD_MODULE_EXPORT
ircd::m::invite(const m::room &room,
                const m::user::id &target,
                const m::user::id &sender,
                json::iov &content)
{
	json::iov event;
	const json::iov::push push[]
	{
		{ event,    { "type",        "m.room.member"  }},
		{ event,    { "sender",      sender           }},
		{ event,    { "state_key",   target           }},
		{ content,  { "membership",  "invite"         }},
	};

	return commit(room, event, content);
}

void
ircd::m::on_invite_foreign(const event &event,
                           vm::eval &eval)
{
	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const m::user::id &target
	{
		at<"state_key"_>(event)
	};

	const auto target_host
	{
		target.host()
	};

	if(m::my_host(target_host))
		return;

	const m::room::origins origins
	{
		room_id
	};

	if(origins.has(target_host))
		return;

	const auto eid
	{
		invite_foreign(event)
	};
}

ircd::m::event::id::buf
ircd::m::invite_foreign(const event &event)
try
{
	const auto &event_id
	{
		event.event_id
	};

	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const m::user::id &target
	{
		at<"state_key"_>(event)
	};

	assert(!my(target));
	const unique_buffer<mutable_buffer> bufs
	{
		148_KiB
	};

	json::stack out{bufs};
	json::stack::object top{out};
	{
		char versionbuf[32];
		json::stack::member room_version
		{
			top, "room_version", json::value
			{
				m::version(versionbuf, room_id, std::nothrow), json::STRING
			}
		};
	}

	{
		json::stack::object _event
		{
			top, "event"
		};

		_event.append(event);
	}

	{
		json::stack::array invite_room_state
		{
			top, "invite_room_state"
		};

		const auto append
		{
			[&invite_room_state](const m::event &event)
			{
				invite_room_state.append(event);
			}
		};

		const m::room::state state
		{
			room_id
		};

		state.get(std::nothrow, "m.room.create", "", append);
		state.get(std::nothrow, "m.room.power_levels", "", append);
		state.get(std::nothrow, "m.room.join_rules", "", append);
		state.get(std::nothrow, "m.room.history_visibility", "", append);
		state.get(std::nothrow, "m.room.aliases", my_host(), append);
		state.get(std::nothrow, "m.room.canonical_alias", "", append);
		state.get(std::nothrow, "m.room.avatar", "", append);
		state.get(std::nothrow, "m.room.name", "", append);
		state.get(std::nothrow, "m.room.member", at<"sender"_>(event), append);
	}

	top.~object();
	const string_view &proto
	{
		out.completed()
	};

	const mutable_buffer buf
	{
		bufs + size(proto)
	};

	m::v2::invite::opts opts;
	opts.remote = target.host();
	m::v2::invite request
	{
		room_id, event_id, proto, buf, std::move(opts)
	};

	http::code rcode; try
	{
		request.wait(seconds(10)); //TODO: conf
		rcode = request.get();
	}
	catch(const http::error &e)
	{
		log::error
		{
			"Invite %s to %s :%s :%s",
			string_view{event.event_id},
			string(opts.remote),
			e.what(),
			e.content,
		};

		throw;
	}

	const json::object response
	{
		request
	};

	m::event::id::buf revent_id;
	const m::event &revent
	{
		revent_id, response.at("event")
	};

	if(!verify(revent, target.host()))
		throw m::error
		{
			http::UNAUTHORIZED, "M_INVITE_UNSIGNED",
			"Invitee's host '%s' did not sign the invite.",
			target.host()
		};

	if(!verify(revent, my_host()))
		throw m::error
		{
			http::FORBIDDEN, "M_INVITE_MODIFIED",
			"Invite event no longer verified by our signature."
		};

	log::info
	{
		m::log, "Invite %s in %s accepted by '%s'",
		string_view{event.event_id},
		string_view{room_id},
		string_view{target.host()},
	};

	m::vm::opts vmopts;
	vmopts.infolog_accept = true;

	m::vm::eval(revent, vmopts);
	return revent.event_id;
}
catch(const std::exception &e)
{
	log::error
	{
		"Invite remote %s :%s",
		string_view{event.event_id},
		e.what(),
	};

	throw;
}
