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

using namespace ircd::m;
using namespace ircd;

static resource::response
get__state(client &client,
           const resource::request &request,
           const m::room::state &state);

resource::response
put__state(client &client,
           const resource::request &request,
           const room::id &room_id)
{
	if(request.parv.size() < 3)
		throw m::NEED_MORE_PARAMS
		{
			"'type' path parameter required."
		};

	char type_buf[m::event::TYPE_MAX_SIZE];
	const string_view &type
	{
		url::decode(type_buf, request.parv[2])
	};

	char skey_buf[m::event::STATE_KEY_MAX_SIZE];
	const string_view &state_key
	{
		request.parv.size() > 3?
			url::decode(skey_buf, request.parv[3]):
			string_view{""} // "" is significant
	};

	const json::object &content
	{
		request.content
	};

	const auto event_id
	{
		m::send(room_id, request.user_id, type, state_key, content)
	};

	return resource::response
	{
		client, json::members
		{
			{ "event_id", event_id }
		}
	};
}

resource::response
get__state(client &client,
           const resource::request &request,
           const room::id &room_id)
{
	char type_buf[m::event::TYPE_MAX_SIZE];
	const string_view &type
	{
		request.parv.size() > 2?
			url::decode(type_buf, request.parv[2]):
			string_view{}
	};

	char skey_buf[m::event::STATE_KEY_MAX_SIZE];
	const string_view &state_key
	{
		request.parv.size() > 3?
			url::decode(skey_buf, request.parv[3]):
			string_view{}
	};

	// (non-standard) Allow an event_id to be passed in the query string
	// for reference framing.
	m::event::id::buf event_id;
	if(request.query["event_id"])
		url::decode(event_id, request.query["event_id"]);

	const m::room room
	{
		room_id, event_id
	};

	if(!exists(room))
		throw m::NOT_FOUND
		{
			"Room %s not found.",
			string_view{room_id}
		};

	if(!visible(room, request.user_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted to view %s",
			string_view{room_id}
		};

	const m::room::state state
	{
		room
	};

	if(!type)
		return get__state(client, request, state);

	const m::event::fetch event
	{
		state.get(type, state_key)
	};

	if(!visible(event, request.user_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted to view this state in %s",
			string_view{room_id}
		};

	return resource::response
	{
		client, event.source
	};
}

resource::response
get__state(client &client,
           const resource::request &request,
           const m::room::state &state)
{
	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher()
	};

	json::stack::array top
	{
		out
	};

	state.for_each([&request, &top]
	(const m::event &event)
	{
		if(!visible(event, request.user_id))
			return;

		top.append(event);
	});

	return std::move(response);
}
