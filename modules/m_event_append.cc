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
	"Matrix Event Library :Streaming tools"
};

IRCD_MODULE_EXPORT
ircd::m::event::append::append(json::stack::array &array,
                               const event &event_,
                               const opts &opts)
:boolean{[&]
{
	json::stack::object object
	{
		array
	};

	const bool ret
	{
		append
		{
			object, event_, opts
		}
	};

	return ret;
}}
{
}

IRCD_MODULE_EXPORT
ircd::m::event::append::append(json::stack::object &object,
                               const event &event,
                               const opts &opts)
:boolean{[&]
{
	const bool has_event_idx
	{
		opts.event_idx && *opts.event_idx
	};

	const bool has_client_txnid
	{
		opts.client_txnid && *opts.client_txnid
	};

	const bool has_user
	{
		opts.user_id && opts.user_room
	};

	const bool sender_is_user
	{
		has_user && json::get<"sender"_>(event) == *opts.user_id
	};

	const auto txnid_idx
	{
		!has_client_txnid && sender_is_user && opts.query_txnid?
			opts.user_room->get(std::nothrow, "ircd.client.txnid", event.event_id):
			0UL
	};

	#if defined(RB_DEBUG) && 0
	if(!has_client_txnid && !txnid_idx && sender_is_user && opts.query_txnid)
		log::dwarning
		{
			log, "Could not find transaction_id for %s from %s in %s",
			string_view{event.event_id},
			json::get<"sender"_>(event),
			json::get<"room_id"_>(event)
		};
	#endif

	if(has_event_idx && !defined(json::get<"state_key"_>(event)) && m::redacted(*opts.event_idx))
	{
		log::debug
		{
			log, "Not sending event '%s' because redacted.",
			string_view{event.event_id},
		};

		return false;
	}

	if(!json::get<"state_key"_>(event) && has_user)
	{
		const m::user::ignores ignores{*opts.user_id};
		if(ignores.enforce("events") && ignores.has(json::get<"sender"_>(event)))
		{
			log::debug
			{
				log, "Not sending event '%s' because '%s' is ignored by '%s'",
				string_view{event.event_id},
				json::get<"sender"_>(event),
				string_view{*opts.user_id}
			};

			return false;
		}
	}

	if(!json::get<"event_id"_>(event))
	{
		auto _event(event);
		json::get<"event_id"_>(_event) = event.event_id;
		object.append(_event);
	}
	else object.append(event);

	if(json::get<"state_key"_>(event) && has_event_idx)
	{
		const auto prev_idx
		{
			room::state::prev(*opts.event_idx)
		};

		if(prev_idx)
			m::get(std::nothrow, prev_idx, "content", [&object]
			(const json::object &content)
			{
				json::stack::member
				{
					object, "prev_content", content
				};
			});
	}

	json::stack::object unsigned_
	{
		object, "unsigned"
	};

	json::stack::member
	{
		unsigned_, "age", json::value
		{
			// When the opts give an explicit age, use it.
			opts.age != std::numeric_limits<long>::min()?
				opts.age:

			// If we have depth information, craft a value based on the
			// distance to the head depth; if this is 0 in riot the event will
			// "stick" at the bottom of the timeline. This may be advantageous
			// in the future but for now we make sure the result is non-zero.
			json::get<"depth"_>(event) >= 0 && opts.room_depth && *opts.room_depth >= 0L?
				(*opts.room_depth + 1 - json::get<"depth"_>(event)) + 1:

			// We don't have depth information, so we use the origin_server_ts.
			// It is bad if it conflicts with other appends in the room which
			// did have depth information.
			!opts.room_depth && json::get<"origin_server_ts"_>(event)?
				ircd::time<milliseconds>() - json::get<"origin_server_ts"_>(event):

			// Finally, this special value will eliminate the age altogether
			// during serialization.
			json::undefined_number
		}
	};

	if(has_client_txnid)
		json::stack::member
		{
			unsigned_, "transaction_id", *opts.client_txnid
		};

	if(txnid_idx)
		m::get(std::nothrow, txnid_idx, "content", [&unsigned_]
		(const json::object &content)
		{
			json::stack::member
			{
				unsigned_, "transaction_id", unquote(content.get("transaction_id"))
			};
		});

	return true;
}}
{
}