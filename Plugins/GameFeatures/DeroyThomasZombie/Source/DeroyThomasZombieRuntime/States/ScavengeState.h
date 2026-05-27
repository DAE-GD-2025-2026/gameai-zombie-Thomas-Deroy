#pragma once

#include "CoreMinimal.h"
#include "BaseState.h" 
#include "ScavengeState.generated.h"

class UInventoryComponent;
class UHealthComponent;
class UStaminaComponent;

UCLASS()
class DEROYTHOMASZOMBIERUNTIME_API UScavengeState : public UBaseState
{
	GENERATED_BODY()

public:
	virtual void Enter(USurvivorFSM* FSM) override;
	virtual void Update(float DeltaTime) override;
	virtual void Exit() override;

private:
	void EvaluateInventory();
    
	UPROPERTY()
	UInventoryComponent* Inventory;
	UPROPERTY()
	UHealthComponent* HealthComp;
	UPROPERTY()
	UStaminaComponent* StaminaComp;
};