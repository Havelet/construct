// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

extern ircd::resource rooms_resource;

///////////////////////////////////////////////////////////////////////////////
//
// event.cc
//

ircd::resource::response
get__event(ircd::client &client,
           const ircd::resource::request &request,
           const ircd::m::room::id &room_id);

///////////////////////////////////////////////////////////////////////////////
//
// messages.cc
//

ircd::resource::response
get__messages(ircd::client &,
              const ircd::resource::request &,
              const ircd::m::room::id &);

///////////////////////////////////////////////////////////////////////////////
//
// state.cc
//

ircd::resource::response
get__state(ircd::client &client,
           const ircd::resource::request &request,
           const ircd::m::room::id &room_id);

ircd::resource::response
put__state(ircd::client &client,
           const ircd::resource::request &request,
           const ircd::m::room::id &room_id);

///////////////////////////////////////////////////////////////////////////////
//
// members.cc
//

ircd::resource::response
get__members(ircd::client &client,
             const ircd::resource::request &request,
             const ircd::m::room::id &room_id);

ircd::resource::response
get__joined_members(ircd::client &client,
                    const ircd::resource::request &request,
                    const ircd::m::room::id &room_id);

///////////////////////////////////////////////////////////////////////////////
//
// context.cc
//

ircd::resource::response
get__context(ircd::client &client,
             const ircd::resource::request &request,
             const ircd::m::room::id &room_id);

///////////////////////////////////////////////////////////////////////////////
//
// send.cc
//

ircd::resource::response
put__send(ircd::client &client,
          const ircd::resource::request &request,
          const ircd::m::room::id &room_id);

///////////////////////////////////////////////////////////////////////////////
//
// typing.cc
//

ircd::resource::response
put__typing(ircd::client &client,
            const ircd::resource::request &request,
            const ircd::m::room::id &room_id);

///////////////////////////////////////////////////////////////////////////////
//
// redact.cc
//

ircd::resource::response
put__redact(ircd::client &client,
            const ircd::resource::request &request,
            const ircd::m::room::id &room_id);

ircd::resource::response
post__redact(ircd::client &client,
             const ircd::resource::request &request,
             const ircd::m::room::id &room_id);

///////////////////////////////////////////////////////////////////////////////
//
// receipt.cc
//

ircd::resource::response
post__receipt(ircd::client &,
              const ircd::resource::request &,
              const ircd::m::room::id &);

///////////////////////////////////////////////////////////////////////////////
//
// join.cc
//

ircd::resource::response
post__join(ircd::client &,
           const ircd::resource::request &,
           const ircd::m::room::id &);

///////////////////////////////////////////////////////////////////////////////
//
// invite.cc
//

ircd::resource::response
post__invite(ircd::client &,
             const ircd::resource::request &,
             const ircd::m::room::id &);

///////////////////////////////////////////////////////////////////////////////
//
// leave.cc
//

ircd::resource::response
post__leave(ircd::client &,
            const ircd::resource::request &,
            const ircd::m::room::id &);

///////////////////////////////////////////////////////////////////////////////
//
// forget.cc
//

ircd::resource::response
post__forget(ircd::client &,
             const ircd::resource::request &,
             const ircd::m::room::id &);

///////////////////////////////////////////////////////////////////////////////
//
// kick.cc
//

ircd::resource::response
post__kick(ircd::client &,
           const ircd::resource::request &,
           const ircd::m::room::id &);

///////////////////////////////////////////////////////////////////////////////
//
// ban.cc
//

ircd::resource::response
post__ban(ircd::client &,
          const ircd::resource::request &,
          const ircd::m::room::id &);

///////////////////////////////////////////////////////////////////////////////
//
// unban.cc
//

ircd::resource::response
post__unban(ircd::client &,
            const ircd::resource::request &,
            const ircd::m::room::id &);

///////////////////////////////////////////////////////////////////////////////
//
// read_markers.cc
//

ircd::resource::response
post__read_markers(ircd::client &,
                   const ircd::resource::request &,
                   const ircd::m::room::id &);

///////////////////////////////////////////////////////////////////////////////
//
// initialsync.cc
//

ircd::resource::response
get__initialsync(ircd::client &,
                 const ircd::resource::request &,
                 const ircd::m::room::id &);

///////////////////////////////////////////////////////////////////////////////
//
// report.cc
//

ircd::resource::response
post__report(ircd::client &,
             const ircd::resource::request &,
             const ircd::m::room::id &);

///////////////////////////////////////////////////////////////////////////////
//
// relations.cc
//

ircd::resource::response
get__relations(ircd::client &,
               const ircd::resource::request &,
               const ircd::m::room::id &);
