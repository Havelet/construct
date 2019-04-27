// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"federation event_auth (undocumented)"
};

resource
event_auth_resource
{
	"/_matrix/federation/v1/event_auth/",
	{
		"federation event_auth",
		resource::DIRECTORY,
	}
};

conf::item<size_t>
event_auth_flush_hiwat
{
	{ "name",     "ircd.federation.event_auth.flush.hiwat" },
	{ "default",  16384L                                   },
};

resource::response
get__event_auth(client &client,
                const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"room_id path parameter required"
		};

	m::room::id::buf room_id
	{
		url::decode(room_id, request.parv[0])
	};

	if(request.parv.size() < 2)
		throw m::NEED_MORE_PARAMS
		{
			"event_id path parameter required"
		};

	m::event::id::buf event_id
	{
		url::decode(event_id, request.parv[1])
	};

	const m::room room
	{
		room_id, event_id
	};

	bool visible
	{
		room.visible(request.node_id)
	};

	// make an exception to the visibility for invitee cases.
	if(!visible)
	{
		static const m::event::fetch::opts fopts
		{
			m::event::keys::include {"room_id", "sender", "type", "state_key", "content"}
		};

		const m::event::fetch event
		{
			event_id, fopts
		};

		visible = m::visible(event, request.node_id);
	}

	if(!visible)
		throw m::ACCESS_DENIED
		{
			"You are not permitted to view the room at this event"
		};

	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher(), size_t(event_auth_flush_hiwat)
	};

	json::stack::object top{out};
	json::stack::array auth_chain
	{
		top, "auth_chain"
	};

	const m::event::auth::chain chain
	{
		m::index(event_id)
	};

	m::event::fetch event;
	chain.for_each([&auth_chain, &event]
	(const m::event::idx &event_idx)
	{
		if(seek(event, event_idx, std::nothrow))
			auth_chain.append(event);
	});

	return response;
}

resource::method
method_get
{
	event_auth_resource, "GET", get__event_auth,
	{
		method_get.VERIFY_ORIGIN
	}
};
