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
#define HAVE_IRCD_M_ROOM_POWER_H

/// Interface to the power levels
///
/// This interface focuses specifically on making the power levels accessible
/// to developers for common query and manipulation operations. power_levels
/// is a single state event in the room containing integer thresholds for
/// privileges in the room. It also contains the list of users mapping to an
/// integer threshold. This interface hides the details of that event by
/// presenting single operations which can appear succinctly in IRCd code.
///
/// Users will typically query the operator() which will return true for allow
/// and false for deny. All other calls are slightly lower level and require
/// a bit more knowledge to safely use.
///
/// There are several modes of construction for this object, however they all
/// funnel into gathering the same information to implement the interface.
///
/// The content property of the power levels event is essential. If the user
/// does not provide this directly, or an event::idx of a power_levels event,
/// current state will be queried. If no power_levels event exists or if it
/// does not contain all of the default properties we will supplement the spec
/// defaults such that this interface always returns results (note that it is
/// still liable to throw exceptions for other reasons).
///
/// The mxid of the room creator should be supplemented for correct operation.
/// If this is not provided the interface still functions correctly but some
/// privileges reserved for room creators will not be available when querying
/// with the creator's room_id. This may be essential functionality when no
/// power_levels event exists.
///
struct ircd::m::room::power
{
	using closure = std::function<void (const string_view &, const int64_t &)>;
	using closure_bool = std::function<bool (const string_view &, const int64_t &)>;

	static const int64_t default_creator_level;
	static const int64_t default_power_level;
	static const int64_t default_event_level;
	static const int64_t default_user_level;

	m::room room;
	event::idx power_event_idx {0};
	json::object power_event_content;
	m::id::user room_creator_id;

	bool view(const std::function<void (const json::object &)> &) const;

  public:
	// Iterate a collection usually either "events" or "users" as per spec.
	bool for_each(const string_view &prop, const closure_bool &) const;
	void for_each(const string_view &prop, const closure &) const;

	// Iterates all of the integer levels, excludes the collections.
	bool for_each(const closure_bool &) const;
	void for_each(const closure &) const;

	bool has_level(const string_view &prop) const;
	bool has_collection(const string_view &prop) const;
	bool has_event(const string_view &type) const;
	bool has_user(const m::id::user &) const;

	size_t count(const string_view &prop) const;
	size_t count_collections() const;
	size_t count_levels() const;

	// This suite queries with full defaulting logic as per the spec. These
	// always return suitable results. When determining power to change a state
	// event rather than a non-state event, the state_key must always be
	// defined. If the state_key is a default constructed string_view{} (which
	// means !defined(state_key) and is not the same as string_view{""} for
	// the common state_key="") then the interface considers your query for
	// a non-state event rather than a state_event. Be careful.
	int64_t level(const string_view &prop) const;
	int64_t level_event(const string_view &type, const string_view &state_key) const;
	int64_t level_event(const string_view &type) const;
	int64_t level_user(const m::id::user &) const;

	// all who attain great power and riches make use of either force or fraud"
	bool operator()(const m::id::user &, const string_view &prop, const string_view &type = {}, const string_view &state_key = {}) const;

	explicit power(const json::object &power_event_content, const m::id::user &room_creator_id);
	explicit power(const m::event &power_event, const m::id::user &room_creator_id);
	explicit power(const m::event &power_event, const m::event &create_event);
	power(const m::room &, const event::idx &power_event_idx);
	power(const m::room &);
	power() = default;

	using compose_closure = std::function<void (const string_view &, json::stack::object &)>;
	static json::object compose_content(const mutable_buffer &out, const compose_closure &);
	static json::object default_content(const mutable_buffer &out, const m::id::user &creator);
};
