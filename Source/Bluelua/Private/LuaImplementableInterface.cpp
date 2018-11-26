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

DECLARE_CYCLE_STAT(TEXT("InitLuaObject"), STAT_InitLuaObject, STATGROUP_Bluelua);
DECLARE_CYCLE_STAT(TEXT("ReleaseLuaObject"), STAT_ReleaseLuaObject, STATGROUP_Bluelua);
DECLARE_CYCLE_STAT(TEXT("CleanAllLuaObject"), STAT_CleanAllLuaObject, STATGROUP_Bluelua);
DECLARE_CYCLE_STAT(TEXT("ProcessLuaObjectOverrideEvent"), STAT_ProcessLuaObjectOverrideEvent, STATGROUP_Bluelua);

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

TMap<FLuaState*, TSet<ILuaImplementableInterface*>> ILuaImplementableInterface::LuaImplementableObjects;

void ILuaImplementableInterface::PreInit(TSharedPtr<FLuaState> InLuaState /*= nullptr*/)
{
	OnInit(BindingLuaPath, InLuaState);
}

bool ILuaImplementableInterface::IsLuaBound() const
{
	return (LuaState.IsValid() && ModuleReferanceIndex != LUA_NOREF);
}

bool ILuaImplementableInterface::FetchLuaModule()
{
	if (IsLuaBound())
	{
		lua_rawgeti(LuaState->GetState(), LUA_REGISTRYINDEX, ModuleReferanceIndex);
		if (lua_type(LuaState->GetState(), -1) != LUA_TTABLE)
		{
			lua_pop(LuaState->GetState(), 1);
			return false;
		}

		return true;
	}

	return false;
}

void ILuaImplementableInterface::CleanAllLuaImplementableObject(FLuaState* InLuaState/* = nullptr*/)
{
	SCOPE_CYCLE_COUNTER(STAT_CleanAllLuaObject);

	TArray<ILuaImplementableInterface*> PendingCleanObjects;
	for (auto& Iter : LuaImplementableObjects)
	{
		if (!InLuaState || Iter.Key == InLuaState)
		{
			for (auto& Interface : Iter.Value)
			{
				PendingCleanObjects.Emplace(Interface);
			}
		}
	}

	for (auto& Iter : PendingCleanObjects)
	{
		UObject* Object = Cast<UObject>(Iter);
		if (Object && Object->IsValidLowLevel())
		{
			Iter->OnRelease();
		}
	}

	if (InLuaState)
	{
		LuaImplementableObjects.Remove(InLuaState);
	}
	else
	{
		LuaImplementableObjects.Empty();
	}
}

void ILuaImplementableInterface::PreRegisterLua(const FString& InLuaFilePath)
{
	if (!InLuaFilePath.IsEmpty())
	{
		BindingLuaPath = InLuaFilePath;
	}
}

bool ILuaImplementableInterface::OnInit(const FString& InLuaFilePath, TSharedPtr<FLuaState> InLuaState/* = nullptr*/)
{
	SCOPE_CYCLE_COUNTER(STAT_InitLuaObject);

	if (InLuaFilePath.IsEmpty())
	{
		return false;
	}

	if (!BindingLuaPath.Equals(InLuaFilePath))
	{
		// new lua file
		OnRelease();
	}

	if (IsLuaBound())
	{
		// already init
		return true;
	}

	LuaState = InLuaState.IsValid() ? InLuaState : FBlueluaModule::Get().GetDefaultLuaState();
	if (!LuaState.IsValid())
	{
		return false;
	}

	BindingLuaPath = InLuaFilePath;
	AddToLuaObjectList(LuaState.Get(), this);

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
	SCOPE_CYCLE_COUNTER(STAT_ReleaseLuaObject);

	RemoveFromLuaObjectList(LuaState.Get(), this);

	if (LuaState.IsValid())
	{
		LuaState->RemoveReferenceByOwner(Cast<UObject>(this));

		if (ModuleReferanceIndex != LUA_NOREF)
		{
			luaL_unref(LuaState->GetState(), LUA_REGISTRYINDEX, ModuleReferanceIndex);
		}
	}

	ModuleReferanceIndex = LUA_NOREF;
	LuaState.Reset();
	BindingLuaPath = TEXT("");
}

bool ILuaImplementableInterface::OnProcessEvent(UFunction* Function, void* Parameters)
{
	if (!LuaState.IsValid() || ModuleReferanceIndex == LUA_NOREF
		|| !Function || !Function->HasAnyFunctionFlags(FUNC_BlueprintEvent))
	{
		return false;
	}

	SCOPE_CYCLE_COUNTER(STAT_ProcessLuaObjectOverrideEvent);

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

void ILuaImplementableInterface::AddToLuaObjectList(FLuaState* InLuaState, ILuaImplementableInterface* Object)
{
	if (!Object)
	{
		return;
	}

	LuaImplementableObjects.FindOrAdd(InLuaState).Emplace(Object);
}

void ILuaImplementableInterface::RemoveFromLuaObjectList(FLuaState* InLuaState, ILuaImplementableInterface* Object)
{
	TSet<ILuaImplementableInterface*>* ValuePtr = LuaImplementableObjects.Find(InLuaState);
	if (!ValuePtr)
	{
		return;
	}

	(*ValuePtr).Remove(Object);
}
