#pragma once

#include "CoreMinimal.h"

#include "LuaObjectBase.h"

class BLUELUA_API FLuaUDelegate : public FLuaObjectBase
{
public:
	FLuaUDelegate(UObject* InOwner, void* InSource, UFunction* InFunction, bool InbIsMulticast);
	~FLuaUDelegate();

	static int Push(lua_State* L, UObject* Owner, void* InSource, UFunction* InFunction, bool InbIsMulticast, void* InBuffer = nullptr);

protected:
	static int Index(lua_State* L);
	static int ToString(lua_State* L);
	static int Add(lua_State* L);
	static int Remove(lua_State* L);

	static const char* UDELEGATE_METATABLE;

protected:
	UObject* Owner;
	void* Source;
	UFunction* Function;
	bool bIsMulticast;
};
