#pragma once

#include "CoreMinimal.h"

struct lua_State;

class BLUELUA_API FLuaStackGuard
{
public:
	FLuaStackGuard(lua_State* L);
	FLuaStackGuard(lua_State* L, int32 Top);

	~FLuaStackGuard();

private:
	lua_State* GuardingStack;
	int32 StackTop;
};
