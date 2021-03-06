// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	extern hookfn<vm::eval &> _access_token_delete_hook;
	static void _access_token_delete(const m::event &event, m::vm::eval &eval);
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix device library; modular components."
};

std::map<std::string, long>
IRCD_MODULE_EXPORT
ircd::m::device::count_one_time_keys(const user &user,
                                     const string_view &device_id)
{
	std::map<std::string, long> ret;
	for_each(user, device_id, [&ret]
	(const string_view &type)
	{
		if(!startswith(type, "one_time_key|"))
			return true;

		const auto &[prefix, ident]
		{
			split(type, '|')
		};

		const auto &[algorithm, name]
		{
			split(ident, ':')
		};

		assert(prefix == "one_time_key");
		assert(!empty(algorithm));
		assert(!empty(ident));
		assert(!empty(name));

		auto it(ret.lower_bound(algorithm));
		if(it == end(ret) || it->first != algorithm)
			it = ret.emplace_hint(it, algorithm, 0L);

		auto &count(it->second);
		++count;
		return true;
	});

	return ret;
}

bool
IRCD_MODULE_EXPORT
ircd::m::device::set(const m::user &user,
                     const device &device)
{
	const user::room user_room{user};
	const string_view &device_id
	{
		json::at<"device_id"_>(device)
	};

	json::for_each(device, [&user, &user_room, &device_id]
	(const auto &prop, auto &&val)
	{
		if(!json::defined(json::value(val)))
			return;

		char buf[m::event::TYPE_MAX_SIZE];
		const string_view type{fmt::sprintf
		{
			buf, "ircd.device.%s", prop
		}};

		m::send(user_room, user, type, device_id, json::members
		{
			{ "", val }
		});
	});

	return true;
}

bool
IRCD_MODULE_EXPORT
ircd::m::device::set(const m::user &user,
                     const string_view &id,
                     const string_view &prop,
                     const string_view &val)
{
	char buf[m::event::TYPE_MAX_SIZE];
	const string_view type{fmt::sprintf
	{
		buf, "ircd.device.%s", prop
	}};

	const user::room user_room{user};
	m::send(user_room, user, type, id, json::members
	{
		{ "", val }
	});

	return true;
}

/// To delete a device we iterate the user's room state for all types matching
/// ircd.device.* (and ircd.device) which have a state_key of the device_id.
/// Those events are redacted which removes them from appearing in the state.
bool
IRCD_MODULE_EXPORT
ircd::m::device::del(const m::user &user,
                     const string_view &id)
{
	const user::room user_room{user};
	const room::state state{user_room};
	const room::state::type_prefix type
	{
		"ircd.device."
	};

	state.for_each(type, [&user, &id, &user_room, &state]
	(const string_view &type, const string_view &, const event::idx &)
	{
		const auto event_idx
		{
			state.get(std::nothrow, type, id)
		};

		const auto event_id
		{
			m::event_id(event_idx, std::nothrow)
		};

		if(event_id)
			m::redact(user_room, user, event_id, "deleted");

		return true;
	});

	return true;
}

/// Deletes the access_token associated with a device when the device
/// (specifically the access_token_id property of that device) is deleted.
decltype(ircd::m::_access_token_delete_hook)
ircd::m::_access_token_delete_hook
{
	_access_token_delete,
	{
		{ "_site",   "vm.effect"         },
		{ "type",    "m.room.redaction"  },
		{ "origin",  my_host()           },
	}
};

void
ircd::m::_access_token_delete(const m::event &event,
                              m::vm::eval &eval)
{
	const auto &target(json::get<"redacts"_>(event));
	if(!target)
		return;

	char buf[std::max(m::event::TYPE_MAX_SIZE, m::id::MAX_SIZE)];
	if(m::get(std::nothrow, target, "type", buf) != "ircd.device.access_token_id"_sv)
		return;

	if(m::get(std::nothrow, target, "sender", buf) != at<"sender"_>(event))
		return;

	m::get(std::nothrow, target, "content", [&event, &target, &buf]
	(const json::object &content)
	{
		const event::id &token_event_id
		{
			unquote(content.at(""))
		};

		m::redact(m::user::tokens, at<"sender"_>(event), token_event_id, "device deleted");
	});
};

bool
IRCD_MODULE_EXPORT
ircd::m::device::has(const m::user &user,
                     const string_view &id)
{
	const user::room user_room{user};
	const room::state state{user_room};
	const room::state::type_prefix type
	{
		"ircd.device."
	};

	bool ret(false);
	state.for_each(type, [&state, &id, &ret]
	(const string_view &type, const string_view &, const event::idx &)
	{
		ret = state.has(type, id);
		return !ret;
	});

	return ret;
}

bool
IRCD_MODULE_EXPORT
ircd::m::device::has(const m::user &user,
                     const string_view &id,
                     const string_view &prop)
{
	bool ret{false};
	get(std::nothrow, user, id, prop, [&ret]
	(const string_view &value)
	{
		ret = !empty(value);
	});

	return ret;
}

bool
IRCD_MODULE_EXPORT
ircd::m::device::get(std::nothrow_t,
                     const m::user &user,
                     const string_view &id,
                     const string_view &prop,
                     const closure &closure)
{
	char buf[m::event::TYPE_MAX_SIZE];
	const string_view type{fmt::sprintf
	{
		buf, "ircd.device.%s", prop
	}};

	const m::user::room user_room{user};
	const m::room::state state{user_room};
	const auto event_idx
	{
		state.get(std::nothrow, type, id)
	};

	return m::get(std::nothrow, event_idx, "content", [&closure]
	(const json::object &content)
	{
		const string_view &value
		{
			content.get("")
		};

		closure(value);
	});
}

bool
IRCD_MODULE_EXPORT
ircd::m::device::for_each(const m::user &user,
                          const string_view &device_id,
                          const closure_bool &closure)
{
	const m::user::room user_room{user};
	const m::room::state state{user_room};
	const room::state::type_prefix type
	{
		"ircd.device."
	};

	return state.for_each(type, [&state, &device_id, &closure]
	(const string_view &type, const string_view &, const event::idx &)
	{
		const string_view &prop
		{
			lstrip(type, "ircd.device.")
		};

		return state.has(type, device_id)?
			closure(prop):
			true;
	});
}

bool
IRCD_MODULE_EXPORT
ircd::m::device::for_each(const m::user &user,
                          const closure_bool &closure)
{
	const m::user::room user_room
	{
		user
	};

	const m::room::state state
	{
		user_room
	};

	return state.for_each("ircd.device.device_id", [&closure]
	(const string_view &, const string_view &state_key, const event::idx &)
	{
		return closure(state_key);
	});
}

ircd::m::device::id::buf
IRCD_MODULE_EXPORT
ircd::m::device::access_token_to_id(const string_view &token)
{
	id::buf ret;
	access_token_to_id(token, [&ret]
	(const string_view &device_id)
	{
		ret = device_id;
	});

	return ret;
}

bool
IRCD_MODULE_EXPORT
ircd::m::device::access_token_to_id(const string_view &token,
                                    const closure &closure)
{
	const m::room::state &state{m::user::tokens};
	const m::event::idx &event_idx
	{
		state.get(std::nothrow, "ircd.access_token", token)
	};

	bool ret{false};
	const auto device_id{[&closure, &ret]
	(const json::object &content)
	{
		const json::string &device_id
		{
			content["device_id"]
		};

		if(likely(device_id))
		{
			closure(device_id);
			ret = true;
		}
	}};

	if(!event_idx)
		return ret;

	if(!m::get(std::nothrow, event_idx, "content", device_id))
		return ret;

	return ret;
}
