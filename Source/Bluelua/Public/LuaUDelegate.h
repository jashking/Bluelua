#pragma once

#include "CoreMinimal.h"

#include "LuaObjectBase.h"

class ULuaFunctionDelegate;
class FLuaUDelegate;

typedef FLuaUDelegate*(*FCreateDelegateFuncPtr)(lua_State* L, void* InSource, UFunction* InFunction);

class BLUELUA_API FLuaUDelegate : public FLuaObjectBase
{
public:
	FLuaUDelegate(void* InSource, UFunction* InFunction);
	virtual ~FLuaUDelegate();

	static int Push(lua_State* L, void* InSource, UFunction* InFunction, FCreateDelegateFuncPtr CreateDelegateFuncPtr, void* InBuffer = nullptr);
	static bool Fetch(lua_State* L, int32 Index, UFunction* InFunction, FScriptDelegate* InScriptDelegate);

protected:
	static int Index(lua_State* L);
	static int ToString(lua_State* L);
	static int Add(lua_State* L);
	static int Remove(lua_State* L);
	static int Clear(lua_State* L);

	static const char* UDELEGATE_METATABLE;

	virtual void OnAdd(ULuaFunctionDelegate* LuaFunctionDelegate) = 0;
	virtual void OnRemove(ULuaFunctionDelegate* LuaFunctionDelegate) = 0;
	virtual void OnClear() = 0;
	virtual TArray<UObject*> OnGetAllObjects() const = 0;
	virtual FString OnGetName() = 0;

protected:
	void* Source;
	UFunction* Function;
};
