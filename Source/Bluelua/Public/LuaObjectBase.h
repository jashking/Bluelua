#pragma once

#include "CoreMinimal.h"

struct lua_State;

class FLuaState;

class BLUELUA_API FLuaObjectBase
{
public:
	typedef int(*PushPropertyFunction)(lua_State* L, UProperty* Property, void* Params, bool bCopyValue);
	typedef bool(*FetchPropertyFunction)(lua_State* L, UProperty* Property, void* Params, int32 Index);

	static void Init();
	static PushPropertyFunction GetPusher(UClass* Class);
	static FetchPropertyFunction GetFetcher(UClass* Class);

	static int PushProperty(lua_State* L, UProperty* Property, void* Params, bool bCopyValue = true);
	static int PushStructProperty(lua_State* L, UProperty* Property, void* Params, bool bCopyValue = true);
	static int PushEnumProperty(lua_State* L, UProperty* Property, void* Params, bool);
	static int PushClassProperty(lua_State* L, UProperty* Property, void* Params, bool);
	static int PushObjectProperty(lua_State* L, UProperty* Property, void* Params, bool);
	static int PushArrayProperty(lua_State* L, UProperty* Property, void* Params, bool);
	static int PushSetProperty(lua_State* L, UProperty* Property, void* Params, bool);
	static int PushMapProperty(lua_State* L, UProperty* Property, void* Params, bool);
	static int PushMulticastDelegateProperty(lua_State* L, UProperty* Property, void* Params, bool);
	static int PushDelegateProperty(lua_State* L, UProperty* Property, void* Params, bool);

	inline static int Push(lua_State* L, int8 Value);
	inline static int Push(lua_State* L, uint8 Value);
	inline static int Push(lua_State* L, int16 Value);
	inline static int Push(lua_State* L, uint16 Value);
	inline static int Push(lua_State* L, int32 Value);
	inline static int Push(lua_State* L, uint32 Value);
	inline static int Push(lua_State* L, int64 Value);
	inline static int Push(lua_State* L, uint64 Value);
	inline static int Push(lua_State* L, float Value);
	inline static int Push(lua_State* L, double Value);
	inline static int Push(lua_State* L, bool Value);
	inline static int Push(lua_State* L, const FString& Value);
	inline static int Push(lua_State* L, const FText& Value);
	inline static int Push(lua_State* L, const FName& Value);

	static bool FetchProperty(lua_State* L, UProperty* Property, void* Params, int32 Index);
	static bool FetchStructProperty(lua_State* L, UProperty* Property, void* Params, int32 Index);
	static bool FetchEnumProperty(lua_State* L, UProperty* Property, void* Params, int32 Index);
	static bool FetchClassProperty(lua_State* L, UProperty* Property, void* Params, int32 Index);
	static bool FetchObjectProperty(lua_State* L, UProperty* Property, void* Params, int32 Index);
	static bool FetchArrayProperty(lua_State* L, UProperty* Property, void* Params, int32 Index);
	static bool FetchSetProperty(lua_State* L, UProperty* Property, void* Params, int32 Index);
	static bool FetchMapProperty(lua_State* L, UProperty* Property, void* Params, int32 Index);
	static bool FetchMulticastDelegateProperty(lua_State* L, UProperty* Property, void* Params, int32 Index);
	static bool FetchDelegateProperty(lua_State* L, UProperty* Property, void* Params, int32 Index);

	inline static bool Fetch(lua_State* L, int32 Index, int8& Value);
	inline static bool Fetch(lua_State* L, int32 Index, uint8& Value);
	inline static bool Fetch(lua_State* L, int32 Index, int16& Value);
	inline static bool Fetch(lua_State* L, int32 Index, uint16& Value);
	inline static bool Fetch(lua_State* L, int32 Index, int32& Value);
	inline static bool Fetch(lua_State* L, int32 Index, uint32& Value);
	inline static bool Fetch(lua_State* L, int32 Index, int64& Value);
	inline static bool Fetch(lua_State* L, int32 Index, uint64& Value);
	inline static bool Fetch(lua_State* L, int32 Index, float& Value);
	inline static bool Fetch(lua_State* L, int32 Index, double& Value);
	inline static bool Fetch(lua_State* L, int32 Index, bool& Value);
	inline static bool Fetch(lua_State* L, int32 Index, FString& Value);
	inline static bool Fetch(lua_State* L, int32 Index, FText& Value);
	inline static bool Fetch(lua_State* L, int32 Index, FName& Value);

protected:
	static int CallFunction(lua_State* L, UObject* Object, UFunction* Function);
};
