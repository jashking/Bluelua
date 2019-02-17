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

	lua_pop(L, 1);
}
