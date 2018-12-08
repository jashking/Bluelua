// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KismetCompilerMisc.h"

class FKismetCompilerContext;
struct FKismetFunctionContext;

//////////////////////////////////////////////////////////////////////////
// FKCHandler_CallLuaFunction

class FKCHandler_CallLuaFunction : public FNodeHandlingFunctor
{
public:
	FKCHandler_CallLuaFunction(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	/**
	 * Searches for the function referenced by a graph node in the CallingContext class's list of functions,
	 * validates that the wiring matches up correctly, and creates an execution statement.
	 */
	void CreateFunctionCallStatement(FKismetFunctionContext& Context, UEdGraphNode* Node, UEdGraphPin* SelfPin);

	bool IsCalledFunctionPure(UEdGraphNode* Node);
	bool IsCalledFunctionFinal(UEdGraphNode* Node);
	bool IsCalledFunctionFromInterface(UEdGraphNode* Node);

private:
	// Get the name of the function to call from the node
	virtual FString GetFunctionNameFromNode(UEdGraphNode* Node) const;
	UClass* GetCallingContext(FKismetFunctionContext& Context, UEdGraphNode* Node);
	UClass* GetTrueCallingClass(FKismetFunctionContext& Context, UEdGraphPin* SelfPin);

public:

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override;
	virtual void RegisterNet(FKismetFunctionContext& Context, UEdGraphPin* Net) override;
	virtual UFunction* FindFunction(FKismetFunctionContext& Context, UEdGraphNode* Node);
	virtual UFunction* FindReplaceFunction(FKismetFunctionContext& Context, UEdGraphNode* Node);
	virtual void Transform(FKismetFunctionContext& Context, UEdGraphNode* Node) override;
	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override;
	virtual void CheckIfFunctionIsCallable(UFunction* Function, FKismetFunctionContext& Context, UEdGraphNode* Node);
	virtual void AdditionalCompiledStatementHandling(FKismetFunctionContext& Context, UEdGraphNode* Node, FBlueprintCompiledStatement& Statement) {}

protected:
	TMap<UEdGraphPin*, FBPTerminal*> InterfaceTermMap;
};
