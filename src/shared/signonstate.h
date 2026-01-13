#pragma once

enum class eSignonState : int
{
	NONE = 0, // no state yet; about to connect
	CHALLENGE = 1, // client challenging server; all OOB packets
	CONNECTED = 2, // client is connected to server; netchans ready
	NEW = 3, // just got serverinfo and string tables
	PRESPAWN = 4, // received signon buffers
	GETTINGDATA = 5, // respawn-defined signonstate, assumedly this is for persistence
	SPAWN = 6, // ready to receive entity packets
	FIRSTSNAP = 7, // another respawn-defined one
	FULL = 8, // we are fully connected; first non-delta packet received
	CHANGELEVEL = 9, // server is changing level; please wait
};
