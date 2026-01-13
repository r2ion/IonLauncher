#pragma once

#include "engine/bitbuf.h"

class INetMessageHandler;
class INetMessage;
class CNetChan;

class INetMessageHandler
{
public:
	virtual ~INetMessageHandler(void) = 0;
	virtual bool ProcessStringCmd(void* msg) = 0;
	virtual bool ProcessSetConVar(void* msg) = 0;
	virtual bool ProcessSignonState(void* msg) = 0;
};

class INetMessage
{
public:
	virtual	~INetMessage() {};

	// Use these to setup who can hear whose voice.
	// Pass in client indices (which are their ent indices - 1).

	virtual void	SetNetChannel(CNetChan* netchan) = 0; // netchannel this message is from/for
	virtual void	SetReliable( bool state ) = 0;	// set to true if it's a reliable message

	virtual bool	Process( void ) = 0; // calls the recently set handler to process this message

	virtual	bool	ReadFromBuffer( bf_read *buffer ) = 0; // returns true if parsing was OK
	virtual	bool	WriteToBuffer( bf_write *buffer ) = 0;	// returns true if writing was OK

	virtual bool	IsReliable( void ) const = 0;  // true, if message needs reliable handling

	virtual int				GetGroup( void ) const = 0;	// returns net message group of this message
	virtual int				GetType( void ) const = 0; // returns module specific header tag eg svc_serverinfo
	virtual int             MysteryReturn0() const = 0; // dunno what this does, is only in r2, xrefed a bit and usually returns 0.
	virtual const char		*GetName( void ) const = 0;	// returns network message name, eg "svc_serverinfo"
	virtual CNetChan		*GetNetChannel( void ) const = 0;
	virtual const char		*ToString( void ) const = 0; // returns a human readable string about message content
	virtual size_t			GetSize( void ) const = 0;	// returns net message size of this message
};
