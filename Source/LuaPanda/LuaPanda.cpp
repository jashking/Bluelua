// Fill out your copyright notice in the Description page of Project Settings.

#include "LuaPanda.h"

#include "lua.hpp"

#include <string>

IMPLEMENT_MODULE(FLuaPanda, LuaPanda);

void FLuaPanda::StartupModule()
{

}

void FLuaPanda::ShutdownModule()
{

}

void FLuaPanda::SetupLuaPanda(struct lua_State* L)
{
	luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);

	lua_pushcfunction(L, &FLuaPanda::OpenLuaPanda);
	lua_setfield(L, -2, "LuaPanda");

	lua_pushcfunction(L, &FLuaPanda::OpenDebugTools);
	lua_setfield(L, -2, "DebugTools");

	lua_pop(L, 1);
}

int FLuaPanda::OpenLuaPanda(lua_State* L)
{
	static std::string RawLua =
#include "LuaPanda.1.lua.inc"

	RawLua +=
#include "LuaPanda.2.lua.inc"

	RawLua +=
#include "LuaPanda.3.lua.inc"

	RawLua +=
#include "LuaPanda.4.lua.inc"

	RawLua +=
#include "LuaPanda.5.lua.inc"

	luaL_dostring(L, RawLua.c_str());
	return 1;
}

int FLuaPanda::OpenDebugTools(lua_State* L)
{
	static const auto RawLua =
#include "DebugTools.lua.inc"

	luaL_dostring(L, RawLua);
	return 1;
}
