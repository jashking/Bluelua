#include "LuaImplementableInterface.h"

#include "Misc/Paths.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#include "Bluelua.h"
#include "lua.hpp"
#include "LuaObjectBase.h"
#include "LuaState.h"
#include "LuaStackGuard.h"
#include "LuaUObject.h"

struct FLuaAutoCleanGlobal
{
public:
	FLuaAutoCleanGlobal(lua_State* L, const char* InGlobalName)
		: GuardingStack(L)
		, GlobalName(InGlobalName)
	{

	}

	~FLuaAutoCleanGlobal()
	{
		if (GuardingStack)
		{
			lua_pushnil(GuardingStack);
			lua_setglobal(GuardingStack, GlobalName);
		}
	}

private:
	lua_State* GuardingStack;
	const char* GlobalName;
};

bool ILuaImplementableInterface::OnInit(const FString& InLuaFilePath, TSharedPtr<FLuaState> InLuaState/* = nullptr*/)
{
	OnRelease();

	LuaState = InLuaState.IsValid() ? InLuaState : FBlueluaModule::Get().GetDefaultLuaState();
	if (!LuaState.IsValid() || InLuaFilePath.IsEmpty())
	{
		return false;
	}

	lua_State* L = LuaState->GetState();
	FLuaStackGuard StackGuard(L);

	FLuaAutoCleanGlobal AutoCleanGlobal(L, "Super");

	FLuaUObject::Push(L, Cast<UObject>(this));
	lua_setglobal(L, "Super");

	if (!LuaState->DoFile(FPaths::ProjectContentDir() / InLuaFilePath))
	{
		return false;
	}

	if (lua_type(L, -1) != LUA_TTABLE)
	{
		return false;
	}

	ModuleReferanceIndex = luaL_ref(L, LUA_REGISTRYINDEX);

	return true;
}

void ILuaImplementableInterface::OnRelease()
{
	if (LuaState.IsValid() && ModuleReferanceIndex != LUA_NOREF)
	{
		luaL_unref(LuaState->GetState(), LUA_REGISTRYINDEX, ModuleReferanceIndex);
	}

	ModuleReferanceIndex = LUA_NOREF;

	LuaState.Reset();
}

bool ILuaImplementableInterface::OnProcessEvent(UFunction* Function, void* Parameters)
{
	if (!LuaState.IsValid() || ModuleReferanceIndex == LUA_NOREF
		|| !Function || !Function->HasAnyFunctionFlags(FUNC_BlueprintEvent))
	{
		return false;
	}

	lua_State* L = LuaState->GetState();
	FLuaStackGuard Gurad(L);

	lua_rawgeti(L, LUA_REGISTRYINDEX, ModuleReferanceIndex);
	if (lua_type(L, -1) != LUA_TTABLE)
	{
		return false;
	}

	// stack = [Module]
	if (lua_getfield(L, -1, TCHAR_TO_UTF8(*Function->GetName())) != LUA_TFUNCTION)
	{
		return false;
	}

	// stack = [Module, Function]
	lua_pushvalue(L, -2);

	// stack = [Module, Function, Module]
	return LuaState->CallLuaFunction(Function, Parameters);
}
