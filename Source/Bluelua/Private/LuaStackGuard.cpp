#include "LuaStackGuard.h"

#include "lua.hpp"

FLuaStackGuard::FLuaStackGuard(lua_State* L)
	: GuardingStack(L)
	, StackTop(lua_gettop(L))
{

}

FLuaStackGuard::FLuaStackGuard(lua_State* L, int32 Top)
	: GuardingStack(L)
	, StackTop(Top)
{

}

FLuaStackGuard::~FLuaStackGuard()
{
	if (GuardingStack)
	{
		lua_settop(GuardingStack, StackTop);
	}
}
