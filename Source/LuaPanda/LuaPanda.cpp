// Fill out your copyright notice in the Description page of Project Settings.

#include "LuaPanda.h"

#include "lua.hpp"
#include "libpdebug.h"

IMPLEMENT_MODULE(FLuaPanda, LuaPanda);

void FLuaPanda::StartupModule()
{

}

void FLuaPanda::ShutdownModule()
{

}

void FLuaPanda::SetupLuaPanda(struct lua_State* L)
{
	pdebug_init(L);

	luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);

	lua_pushcfunction(L, &FLuaPanda::OpenLuaPanda);
	lua_setfield(L, -2, "LuaPanda");

	lua_pushcfunction(L, &FLuaPanda::OpenDebugTools);
	lua_setfield(L, -2, "DebugTools");

	lua_pop(L, 1);
}

int FLuaPanda::OpenLuaPanda(lua_State* L)
{
	static const auto RawLua1 =
#include "LuaPanda.1.lua.inc"

	static const auto RawLua2 =
#include "LuaPanda.2.lua.inc"

	static const auto RawLua3 =
#include "LuaPanda.3.lua.inc"

	static const auto RawLua4 =
#include "LuaPanda.4.lua.inc"

	static const auto RawLua5 =
#include "LuaPanda.5.lua.inc"

	static const int32 ContentSize = 100 * 1024;
	static ANSICHAR LuaContent[ContentSize] = { 0 };
	if (LuaContent[0] == 0)
	{
		FCStringAnsi::Snprintf(LuaContent, ContentSize, "%s%s%s%s%s", RawLua1, RawLua2, RawLua3, RawLua4, RawLua5);
	}

	luaL_dostring(L, LuaContent);
	return 1;
}

int FLuaPanda::OpenDebugTools(lua_State* L)
{
	static const auto RawLua =
#include "DebugTools.lua.inc"

	luaL_dostring(L, RawLua);
	return 1;
}
