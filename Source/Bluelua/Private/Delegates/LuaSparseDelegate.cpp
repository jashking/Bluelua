#include "LuaSparseDelegate.h"

#include "Runtime/Launch/Resources/Version.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#if ENGINE_MINOR_VERSION >= 23
#include "UObject/SparseDelegate.h"
#endif // ENGINE_MINOR_VERSION >= 23

#include "Bluelua.h"
#include "lua.hpp"
#include "LuaFunctionDelegate.h"
#include "LuaState.h"

FLuaSparseDelegate::FLuaSparseDelegate(void* InSource, UFunction* InFunction)
	: FLuaUDelegate(InSource, InFunction)
{

}

FLuaSparseDelegate::~FLuaSparseDelegate()
{

}

FLuaUDelegate* FLuaSparseDelegate::Create(lua_State* L, void* InSource, UFunction* InFunction)
{
	uint8* UserData = (uint8*)lua_newuserdata(L, sizeof(FLuaSparseDelegate));

	FLuaSparseDelegate* LuaSparseDelegate = new(UserData) FLuaSparseDelegate(InSource, InFunction);

	return LuaSparseDelegate;
}

void FLuaSparseDelegate::OnAdd(ULuaFunctionDelegate* LuaFunctionDelegate)
{
#if ENGINE_MINOR_VERSION >= 23
	FScriptDelegate Delegate;
	Delegate.BindUFunction(LuaFunctionDelegate, ULuaFunctionDelegate::DelegateFunctionName);

	FSparseDelegate* SparseDelegate = reinterpret_cast<FSparseDelegate*>(Source);
	if (SparseDelegate)
	{
		USparseDelegateFunction* SparseDelegateFunc = CastChecked<USparseDelegateFunction>(Function);

		UObject* Parent = FSparseDelegateStorage::ResolveSparseOwner(*SparseDelegate, SparseDelegateFunc->OwningClassName, SparseDelegateFunc->DelegateName);

		SparseDelegate->__Internal_AddUnique(Parent, SparseDelegateFunc->DelegateName, MoveTemp(Delegate));
	}
#endif // ENGINE_MINOR_VERSION >= 23
}

void FLuaSparseDelegate::OnRemove(ULuaFunctionDelegate* LuaFunctionDelegate)
{
#if ENGINE_MINOR_VERSION >= 23
	FScriptDelegate Delegate;
	Delegate.BindUFunction(LuaFunctionDelegate, ULuaFunctionDelegate::DelegateFunctionName);

	FSparseDelegate* SparseDelegate = reinterpret_cast<FSparseDelegate*>(Source);
	if (SparseDelegate)
	{
		USparseDelegateFunction* SparseDelegateFunc = CastChecked<USparseDelegateFunction>(Function);

		UObject* Parent = FSparseDelegateStorage::ResolveSparseOwner(*SparseDelegate, SparseDelegateFunc->OwningClassName, SparseDelegateFunc->DelegateName);

		SparseDelegate->__Internal_Remove(Parent, SparseDelegateFunc->DelegateName, MoveTemp(Delegate));
	}
#endif // ENGINE_MINOR_VERSION >= 23
}

void FLuaSparseDelegate::OnClear()
{
#if ENGINE_MINOR_VERSION >= 23
	FSparseDelegate* SparseDelegate = reinterpret_cast<FSparseDelegate*>(Source);
	if (SparseDelegate)
	{
		USparseDelegateFunction* SparseDelegateFunc = CastChecked<USparseDelegateFunction>(Function);

		UObject* Parent = FSparseDelegateStorage::ResolveSparseOwner(*SparseDelegate, SparseDelegateFunc->OwningClassName, SparseDelegateFunc->DelegateName);

		SparseDelegate->__Internal_Clear(Parent, SparseDelegateFunc->DelegateName);
	}
#endif // ENGINE_MINOR_VERSION >= 23
}

TArray<UObject*> FLuaSparseDelegate::OnGetAllObjects() const
{
	TArray<UObject*> OutputList;

#if ENGINE_MINOR_VERSION >= 23
#else
#endif // ENGINE_MINOR_VERSION >= 23

	return OutputList;
}

FString FLuaSparseDelegate::OnGetName()
{
	return TEXT("SparseDelegate");
}
