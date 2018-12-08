// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallLuaImplementation.generated.h"

/**
 * 
 */
UCLASS()
class BLUELUAEDITOR_API UK2Node_CallLuaImplementation : public UK2Node_CallFunction
{
	GENERATED_BODY()

public:
	//~ Begin EdGraphNode Interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual void PostPlacedNewNode() override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins);
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	//~ End EdGraphNode Interface

	//~ Begin K2Node Interface.
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	//~ End K2Node Interface.

	virtual void SetFromFunction(const UFunction* Function) override;

	UEdGraphPin* GetFunctionNamePin();
	UEdGraphPin* GetCompletedResultPin();

protected:
	static const FName PN_FunctionName;
};
