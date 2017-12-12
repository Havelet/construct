/*
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

using namespace ircd;

const database::descriptor events_event_id_descriptor
{
	// name
	"event_id",

	// explanation
	R"(### protocol note:

	10.1
	The id of event.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_id. This is redundant data but we have to have it for now.
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_type_descriptor
{
	// name
	"type",

	// explanation
	R"(### protocol note:

	10.1
	The type of event. This SHOULD be namespaced similar to Java package naming conventions
	e.g. 'com.example.subdomain.event.type'.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_content_descriptor
{
	// name
	"content",

	// explanation
	R"(### protocol note:

	10.1
	The fields in this object will vary depending on the type of event. When interacting
	with the REST API, this is the HTTP body.

	### developer note:
	Since events must not exceed 65 KB the maximum size for the content is the remaining
	space after all the other fields for the event are rendered.

	key is event_id
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_room_id_descriptor
{
	// name
	"room_id",

	// explanation
	R"(### protocol note:

	10.2 (apropos room events)
	Required. The ID of the room associated with this event.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_sender_descriptor
{
	// name
	"sender",

	// explanation
	R"(### protocol note:

	10.2 (apropos room events)
	Required. Contains the fully-qualified ID of the user who sent this event.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_state_key_descriptor
{
	// name
	"state_key",

	// explanation
	R"(### protocol note:

	10.3 (apropos room state events)
	A unique key which defines the overwriting semantics for this piece of room state.
	This value is often a zero-length string. The presence of this key makes this event a
	State Event. The key MUST NOT start with '_'.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_origin_descriptor
{
	// name
	"origin",

	// explanation
	R"(### protocol note:

	FEDERATION 4.1
	DNS name of homeserver that created this PDU

	### developer note:
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_origin_server_ts_descriptor
{
	// name
	"origin_server_ts",

	// explanation
	R"(### protocol note:

	FEDERATION 4.1
	Timestamp in milliseconds on origin homeserver when this PDU was created.

	### developer note:
	key is event_id
	value is a machine integer (binary)

	TODO: consider unsigned rather than time_t because of millisecond precision

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(time_t)
	}
};

const database::descriptor events_unsigned_descriptor
{
	// name
	"unsigned",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_signatures_descriptor
{
	// name
	"signatures",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_auth_events_descriptor
{
	// name
	"auth_events",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id.
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_depth_descriptor
{
	// name
	"depth",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id value is long integer
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(int64_t)
	}
};

const database::descriptor events_hashes_descriptor
{
	// name
	"hashes",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id.
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_membership_descriptor
{
	// name
	"membership",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id.
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_prev_events_descriptor
{
	// name
	"prev_events",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id.
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_prev_state_descriptor
{
	// name
	"prev_state",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id.
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

/// prefix transform for event_id suffixes
///
/// This transform expects a concatenation ending with an event_id which means
/// the prefix can be the same for multiple event_id's; therefor we can find
/// or iterate "event_id in X" where X is some key like a room_id
///
const ircd::db::prefix_transform event_id_in
{
	"event_id in",
	[](const string_view &key)
	{
		return key.find('$') != key.npos;
	},
	[](const string_view &key)
	{
		return rsplit(key, '$').first;
	}
};

const database::descriptor event_id_in_sender
{
	// name
	"event_id in sender",

	// explanation
	R"(### developer note:

	key is "@sender$event_id"
	the prefix transform is in effect. this column indexes events by
	sender offering an iterable bound of the index prefixed by sender

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	event_id_in,
};

const database::descriptor event_id_in_room_id
{
	// name
	"event_id in room_id",

	// explanation
	R"(### developer note:

	key is "!room_id$event_id"
	the prefix transform is in effect. this column indexes events by
	room_id offering an iterable bound of the index prefixed by room_id

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	},

	// options
	{},

	// comparator - sorts from highest to lowest
	{}, //ircd::db::reverse_cmp_string_view{},

	// prefix transform
	event_id_in,
};

/// prefix transform for origin in
///
/// This transform expects a concatenation ending with an origin which means
/// the prefix can be the same for multiple origins; therefor we can find
/// or iterate "origin in X" where X is some repeated prefix
///
/// TODO: strings will have character conflicts. must address
const ircd::db::prefix_transform origin_in
{
	"origin in",
	[](const string_view &key)
	{
		return has(key, ":::");
		//return key.find(':') != key.npos;
	},
	[](const string_view &key)
	{
		return split(key, ":::").first;
		//return rsplit(key, ':').first;
	}
};

const database::descriptor origin_in_room_id
{
	// name
	"origin in room_id",

	// explanation
	R"(### developer note:

	key is "!room_id:origin"
	the prefix transform is in effect. this column indexes origins in a
	room_id offering an iterable bound of the index prefixed by room_id

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	},

	// options
	{},

	// comparator - sorts from highest to lowest
	{}, //ircd::db::reverse_cmp_string_view{},

	// prefix transform
	origin_in,
};

/// prefix transform for room_id
///
/// This transform expects a concatenation ending with a room_id which means
/// the prefix can be the same for multiple room_id's; therefor we can find
/// or iterate "room_id in X" where X is some repeated prefix
///
const ircd::db::prefix_transform room_id_in
{
	"room_id in",
	[](const string_view &key)
	{
		return key.find('!') != key.npos;
	},
	[](const string_view &key)
	{
		return rsplit(key, '!').first;
	}
};

const database::descriptor event_id_for_room_id_in_sender
{
	// name
	"event_id for room_id in sender",

	// explanation
	R"(### developer note:

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	room_id_in,
};

/// prefix transform for type,state_key in room_id
///
/// This transform is special for concatenating room_id with type and state_key
/// in that order with prefix being the room_id (this may change to room_id+
/// type
///
/// TODO: arbitrary type strings will have character conflicts. must address
/// TODO: with grammars.
const ircd::db::prefix_transform type_state_key_in_room_id
{
	"type,state_key in room_id",
	[](const string_view &key)
	{
		return key.find("..") != key.npos;
	},
	[](const string_view &key)
	{
		return split(key, "..").first;
	}
};

const database::descriptor event_id_for_type_state_key_in_room_id
{
	// name
	"event_id for type,state_key in room_id",

	// explanation
	R"(### developer note:

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	type_state_key_in_room_id
};

const database::descriptor prev_event_id_for_event_id_in_room_id
{
	// name
	"prev_event_id for event_id in room_id",

	// explanation
	R"(### developer note:

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	event_id_in
};

/// prefix transform for event_id in room_id,type,state_key
///
/// This transform is special for concatenating room_id with type and state_key
/// and event_id in that order with prefix being the room_id,type,state_key. This
/// will index multiple event_ids with the same type,state_key in a room which
/// allows for a temporal depth to the database; event_id for type,state_key only
/// resolves to a single latest event and overwrites itself as per the room state
/// algorithm whereas this can map all of them and then allows for tracing.
///
/// TODO: arbitrary type strings will have character conflicts. must address
/// TODO: with grammars.
const ircd::db::prefix_transform event_id_in_room_id_type_state_key
{
	"event_id in room_id,type_state_key",
	[](const string_view &key)
	{
		return has(key, '$');
	},
	[](const string_view &key)
	{
		return split(key, '$').first;
	}
};

const database::descriptor prev_event_id_for_type_state_key_event_id_in_room_id
{
	// name
	"prev_event_id for type,state_key,event_id in room_id",

	// explanation
	R"(### developer note:

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	event_id_in_room_id_type_state_key
};

const database::description events_description
{
	{ "default" },

	// These columns directly represent event fields indexed by event_id and
	// the value is the actual event values. Some values may be JSON, like
	// content.
	events_event_id_descriptor,
	events_type_descriptor,
	events_content_descriptor,
	events_room_id_descriptor,
	events_sender_descriptor,
	events_state_key_descriptor,
	events_origin_descriptor,
	events_origin_server_ts_descriptor,
	events_unsigned_descriptor,
	events_signatures_descriptor,
	events_auth_events_descriptor,
	events_depth_descriptor,
	events_hashes_descriptor,
	events_membership_descriptor,
	events_prev_events_descriptor,
	events_prev_state_descriptor,

	// These columns are metadata composed from the event data. Each column has
	// a different approach specific to what the query is and value being sought.

	// (sender, event_id) => ()
	// All events for a sender
	// * broad but useful in cases
	event_id_in_sender,

	// (room_id, event_id) => ()
	// All events for a room
	// * broad but useful in cases
	// * ?eliminate for prev_event?
	event_id_in_room_id,

	// (room_id, origin) => ()
	// All origins for a room
	origin_in_room_id,

	// (sender, room_id) => (event_id)
	// The _last written_ event from a sender in a room
	event_id_for_room_id_in_sender,

	// (room_id, type, state_key) => (event_id)
	// The _last written_ event of type + state_key in a room
	// * Proper for room state algorithm, but only works in the present based
	// on our subjective tape order.
	event_id_for_type_state_key_in_room_id,

	// (room_id, event_id) => (prev event_id)
	// Events in a room resolving to the previous event in a room in
	// our subjective euclidean tape order.
	prev_event_id_for_event_id_in_room_id,

	// (room_id, type, state_key, event_id) => (prev event_id)
	// Events of a (type, state_key) in a room resolving to the previous event
	// of (type, state_key) in a room in our subjective euclidean tape order.
	prev_event_id_for_type_state_key_event_id_in_room_id,
};

std::shared_ptr<database> events_database
{
	std::make_shared<database>("events"s, ""s, events_description)
};

mapi::header IRCD_MODULE
{
	"Hosts the 'events' database"
};
