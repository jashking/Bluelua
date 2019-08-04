#pragma once

#include "CoreMinimal.h"

#include "LuaUDelegate.h"

class BLUELUA_API FLuaMulticastScriptDelegate : public FLuaUDelegate
{
public:
	FLuaMulticastScriptDelegate(void* InSource, UFunction* InFunction);
	virtual ~FLuaMulticastScriptDelegate();

	static FLuaUDelegate* Create(lua_State* L, void* InSource, UFunction* InFunction);

protected:
	virtual void OnAdd(ULuaFunctionDelegate* LuaFunctionDelegate) override;
	virtual void OnRemove(ULuaFunctionDelegate* LuaFunctionDelegate) override;
	virtual void OnClear() override;
	virtual TArray<UObject*> OnGetAllObjects() const override;
	virtual FString OnGetName() override;
};
