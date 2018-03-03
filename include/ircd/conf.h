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
#define HAVE_IRCD_CONF_H

namespace ircd::conf
{
	template<class T = void> struct item;  // doesn't exist
	template<class T> struct value;        // abstraction for carrying item value
	template<> struct item<void>;          // base class of all conf items
	template<> struct item<std::string>;
	template<> struct item<uint64_t>;
	template<> struct item<int64_t>;
	template<> struct item<seconds>;

	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, not_found)
	IRCD_EXCEPTION(error, bad_value)

	extern const std::string &config; //TODO: X
	extern std::map<string_view, item<> *> items;

	string_view get(const string_view &key, const mutable_buffer &out);
	bool set(const string_view &key, const string_view &value);
	bool set(std::nothrow_t, const string_view &key, const string_view &value);

	void init(const string_view &configfile);
}

/// Conf item base class. You don't create this directly; use one of the
/// derived templates instead.
template<>
struct ircd::conf::item<void>
{
	json::strung feature_;
	json::object feature;
	string_view name;

	virtual string_view get(const mutable_buffer &) const;
	virtual bool set(const string_view &);

	item(const json::members &);
	item(item &&) = delete;
	item(const item &) = delete;
	virtual ~item() noexcept;
};

/// Conf item value abstraction. If possible, the conf item will also
/// inherit from this template to deduplicate functionality between
/// conf items which contain similar classes of values.
template<class T>
struct ircd::conf::value
{
	using value_type = T;

	T _value;

	operator const T &() const
	{
		return _value;
	}

	template<class... A>
	value(A&&... a)
	:_value{std::forward<A>(a)...}
	{}
};

template<>
struct ircd::conf::item<std::string>
:conf::item<>
,conf::value<std::string>
{
	operator string_view() const
	{
		return _value;
	}

	string_view get(const mutable_buffer &out) const override
	{
		return { data(out), _value.copy(data(out), size(out)) };
	}

	bool set(const string_view &s) override
	{
		_value = std::string{s};
		return true;
	}

	item(const json::members &memb)
	:conf::item<>{memb}
	,value{unquote(feature.get("default"))}
	{}
};

template<>
struct ircd::conf::item<uint64_t>
:conf::item<>
,conf::value<uint64_t>
{
	string_view get(const mutable_buffer &out) const override
	{
		return lex_cast(_value, out);
	}

	bool set(const string_view &s) override
	{
		_value = lex_cast<uint64_t>(s);
		return true;
	}

	item(const json::members &memb)
	:conf::item<>{memb}
	,value{feature.get("default", 0UL)}
	{}
};

template<>
struct ircd::conf::item<int64_t>
:conf::item<>
,conf::value<int64_t>
{
	string_view get(const mutable_buffer &out) const override
	{
		return lex_cast(_value, out);
	}

	bool set(const string_view &s) override
	{
		_value = lex_cast<int64_t>(s);
		return true;
	}

	item(const json::members &memb)
	:conf::item<>{memb}
	,value{feature.get("default", 0L)}
	{}
};

template<>
struct ircd::conf::item<ircd::seconds>
:conf::item<>
,conf::value<seconds>
{
	string_view get(const mutable_buffer &out) const override
	{
		return lex_cast(_value, out);
	}

	bool set(const string_view &s) override
	{
		_value = lex_cast<seconds>(s);
		return true;
	}

	item(const json::members &memb)
	:conf::item<>{memb}
	,value{feature.get("default", 0L)}
	{}
};
