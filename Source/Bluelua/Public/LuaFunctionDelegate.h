#pragma once

#include "CoreMinimal.h"

#include "LuaFunctionDelegate.generated.h"

class FLuaState;
struct lua_State;

UCLASS()
class BLUELUA_API ULuaFunctionDelegate : public UObject
{
	GENERATED_BODY()

public:
	virtual void ProcessEvent(UFunction* Function, void* Parameters) override;
	virtual void BeginDestroy() override;

	static ULuaFunctionDelegate* Create(UObject* InDelegateOwner, TSharedPtr<FLuaState> InLuaState, UFunction* InSignatureFunction, int InLuaFunctionIndex);
	static ULuaFunctionDelegate* Fetch(lua_State* L, int32 Index);
	static int CreateFunctionDelegate(lua_State* L);
	static int DeleteFunctionDelegate(lua_State* L);

	void BindLuaState(TSharedPtr<FLuaState> InLuaState);
	void BindLuaFunction(UFunction* InSignatureFunction, int InLuaFunctionIndex);
	void BindLuaFunctionOwner(int InLuaFunctionOwerIndex);
	void BindSignatureFunction(UFunction* InSignatureFunction);
	bool IsBound() const;

	UFUNCTION(BlueprintCallable)
	void Clear();

	UFUNCTION()
	void NeverUsed() {}

	static const TCHAR* DelegateFunctionName;

protected:
	TWeakPtr<FLuaState> LuaState;

	FString LastSignatureFunctionName;
	UFunction* SignatureFunction = nullptr;
	int LuaFunctionIndex = -2;
	int LuaFunctionOwnerIndex = -2;
};
