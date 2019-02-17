// Fill out your copyright notice in the Description page of Project Settings.

#include "LibLuasocket.h"

#include "lua.hpp"

extern "C" {
#include "luasocket.h"
#include "mime.h"
}

IMPLEMENT_MODULE(FLibLuasocketModule, LibLuasocket);

void FLibLuasocketModule::StartupModule()
{

}

void FLibLuasocketModule::ShutdownModule()
{

}

void FLibLuasocketModule::SetupLuasocket(struct lua_State* L)
{
	luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);

	lua_pushcfunction(L, luaopen_socket_core);
	lua_setfield(L, -2, "socket.core");

	lua_pushcfunction(L, luaopen_mime_core);
	lua_setfield(L, -2, "mime.core");

	lua_pushcfunction(L, &FLibLuasocketModule::OpenLuaSocketFtp);
	lua_setfield(L, -2, "socket.ftp");

	lua_pushcfunction(L, &FLibLuasocketModule::OpenLuaSocketHeaders);
	lua_setfield(L, -2, "socket.headers");

	lua_pushcfunction(L, &FLibLuasocketModule::OpenLuaSocketHttp);
	lua_setfield(L, -2, "socket.http");

	lua_pushcfunction(L, &FLibLuasocketModule::OpenLuaSocketLtn12);
	lua_setfield(L, -2, "ltn12");

	lua_pushcfunction(L, &FLibLuasocketModule::OpenLuaSocketMbox);
	lua_setfield(L, -2, "mbox");

	lua_pushcfunction(L, &FLibLuasocketModule::OpenLuaSocketMime);
	lua_setfield(L, -2, "mime");

	lua_pushcfunction(L, &FLibLuasocketModule::OpenLuaSocketSmtp);
	lua_setfield(L, -2, "socket.smtp");

	lua_pushcfunction(L, &FLibLuasocketModule::OpenLuaSocketSocket);
	lua_setfield(L, -2, "socket");

	lua_pushcfunction(L, &FLibLuasocketModule::OpenLuaSocketTp);
	lua_setfield(L, -2, "socket.tp");

	lua_pushcfunction(L, &FLibLuasocketModule::OpenLuaSocketUrl);
	lua_setfield(L, -2, "socket.url");

	lua_pop(L, 1);
}

int FLibLuasocketModule::OpenLuaSocketFtp(lua_State* L)
{
	static const auto RawLua =
#include "ftp.lua.inc"

	luaL_dostring(L, RawLua);
	return 1;
}

int FLibLuasocketModule::OpenLuaSocketHeaders(lua_State* L)
{
	static const auto RawLua =
#include "headers.lua.inc"

	luaL_dostring(L, RawLua);
	return 1;
}

int FLibLuasocketModule::OpenLuaSocketHttp(lua_State* L)
{
	static const auto RawLua =
#include "http.lua.inc"

	luaL_dostring(L, RawLua);
	return 1;
}

int FLibLuasocketModule::OpenLuaSocketLtn12(lua_State* L)
{
	static const auto RawLua =
#include "ltn12.lua.inc"

	luaL_dostring(L, RawLua);
	return 1;
}

int FLibLuasocketModule::OpenLuaSocketMbox(lua_State* L)
{
	static const auto RawLua =
#include "mbox.lua.inc"

	luaL_dostring(L, RawLua);
	return 1;
}

int FLibLuasocketModule::OpenLuaSocketMime(lua_State* L)
{
	static const auto RawLua =
#include "mime.lua.inc"

	luaL_dostring(L, RawLua);
	return 1;
}

int FLibLuasocketModule::OpenLuaSocketSmtp(lua_State* L)
{
	static const auto RawLua =
#include "smtp.lua.inc"

	luaL_dostring(L, RawLua);
	return 1;
}

int FLibLuasocketModule::OpenLuaSocketSocket(lua_State* L)
{
	static const auto RawLua =
#include "socket.lua.inc"

	luaL_dostring(L, RawLua);
	return 1;
}

int FLibLuasocketModule::OpenLuaSocketTp(lua_State* L)
{
	static const auto RawLua =
#include "tp.lua.inc"

	luaL_dostring(L, RawLua);
	return 1;
}

int FLibLuasocketModule::OpenLuaSocketUrl(lua_State* L)
{
	static const auto RawLua =
#include "url.lua.inc"

	luaL_dostring(L, RawLua);
	return 1;
}
