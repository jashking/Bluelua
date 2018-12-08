// Fill out your copyright notice in the Description page of Project Settings.

#include "K2Node_CallLuaImplementation.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "GraphEditorSettings.h"
#include "Kismet2/CompilerResultsLog.h"

#include "CallLuaFunctionHandler.h"

#define LOCTEXT_NAMESPACE "K2Node"

const FName UK2Node_CallLuaImplementation::PN_FunctionName(TEXT("FunctionName"));

FText UK2Node_CallLuaImplementation::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
	{
		return FText::FromString(TEXT("Call Lua Function"));
	}

	UFunction* Function = FunctionReference.ResolveMember<UFunction>(GetBlueprintClassFromNode());
	FText FunctionName;

	if (Function)
	{
		FunctionName = GetUserFacingFunctionName(Function);
	}
	//else if (GEditor && GetDefault<UEditorStyleSettings>()->bShowFriendlyNames)
	//{
	//	FunctionName = FText::FromString(FName::NameToDisplayString(FunctionReference.GetMemberName().ToString(), false));
	//}

	FFormatNamedArguments Args;
	Args.Add(TEXT("FunctionName"), FunctionName);
	return FText::Format(LOCTEXT("CallLuaImplementation", "LuaImplementation: {FunctionName}"), Args);
}

FLinearColor UK2Node_CallLuaImplementation::GetNodeTitleColor() const
{
	return GetDefault<UGraphEditorSettings>()->ParentFunctionCallNodeTitleColor;
}

void UK2Node_CallLuaImplementation::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	UEdGraphPin* NamePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Name, UK2Node_CallLuaImplementation::PN_FunctionName);
	NamePin->bNotConnectable = true;

	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Boolean, UEdGraphSchema_K2::PN_Completed);

	//const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	//UEdGraphPin* SelfPin = Schema->FindSelfPin(*this, EGPD_Input);
	//if (SelfPin)
	//{
	//	SelfPin->bHidden = true;
	//}
}

void UK2Node_CallLuaImplementation::SetFromFunction(const UFunction* Function)
{
	Super::SetFromFunction(Function);
	//if (Function != NULL)
	//{
	//	bIsPureFunc = Function->HasAnyFunctionFlags(FUNC_BlueprintPure);
	//	bIsConstFunc = Function->HasAnyFunctionFlags(FUNC_Const);

	//	UClass* OwnerClass = Function->GetOwnerClass();

	//	FGuid FunctionGuid;
	//	if (OwnerClass != nullptr)
	//	{
	//		OwnerClass = OwnerClass->GetAuthoritativeClass();
	//		UBlueprint::GetGuidFromClassByFieldName<UFunction>(OwnerClass, Function->GetFName(), FunctionGuid);
	//	}

	//	FunctionReference.SetDirect(Function->GetFName(), FunctionGuid, OwnerClass, /*bIsConsideredSelfContext =*/false);
	//}
}

UEdGraphPin* UK2Node_CallLuaImplementation::GetFunctionNamePin()
{
	UEdGraphPin* Pin = FindPin(UK2Node_CallLuaImplementation::PN_FunctionName);
	check(Pin);
	return Pin;
}

UEdGraphPin* UK2Node_CallLuaImplementation::GetCompletedResultPin()
{
	UEdGraphPin* Pin = FindPin(UEdGraphSchema_K2::PN_Completed);
	check(Pin);
	return Pin;
}

void UK2Node_CallLuaImplementation::PostPlacedNewNode()
{
	// We don't want to check if our function exists in the current scope

	UK2Node::PostPlacedNewNode();
}

void UK2Node_CallLuaImplementation::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	UEdGraphPin* FunctionNamePin = GetFunctionNamePin();
	if (Pin == FunctionNamePin)
	{
		UClass* Class = GetBlueprintClassFromNode();
		UFunction* Function = Class->FindFunctionByName(*FunctionNamePin->GetDefaultAsString());
		if (Function)
		{
			SetFromFunction(Function);

			BreakAllNodeLinks();
			ReconstructNode();
		}
	}
}

void UK2Node_CallLuaImplementation::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	Super::ReallocatePinsDuringReconstruction(OldPins);
}

class FNodeHandlingFunctor* UK2Node_CallLuaImplementation::CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_CallLuaFunction(CompilerContext);
}

void UK2Node_CallLuaImplementation::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

void UK2Node_CallLuaImplementation::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	UEdGraphPin* FunctionNamePin = FindPin(UK2Node_CallLuaImplementation::PN_FunctionName);

	UFunction* Function = GetTargetFunction();
	if (Function)
	{
		const FString FunctName = FunctionReference.GetMemberName().ToString();
		const FString ConfigFunctName = FunctionNamePin ? FunctionNamePin->GetDefaultAsString() : TEXT("");

		if (!FunctName.Equals(ConfigFunctName))
		{
			FText const WarningFormat = LOCTEXT("FunctionNotFoundFmt", "Function name pin: \"{0}\" not match with function: \"{1}\" in @@");
			MessageLog.Error(*FText::Format(WarningFormat, FText::FromString(ConfigFunctName), FText::FromString(FunctName)).ToString(), this);
		}
	}
}

#undef LOCTEXT_NAMESPACE
