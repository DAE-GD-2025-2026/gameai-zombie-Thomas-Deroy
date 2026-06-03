#pragma once

#include "CoreMinimal.h"
#include "BaseStateDeroyThomas.h" 
#include "ScavengeStateDeroyThomas.generated.h"

class UInventoryComponent;
class UHealthComponent;
class UStaminaComponent;

UCLASS()
class DEROYTHOMASZOMBIERUNTIME_API UScavengeStateDeroyThomas : public UBaseStateDeroyThomas
{
	GENERATED_BODY()

public:
	virtual void Enter(USurvivorFSMDeroyThomas* FSM) override;
	virtual void Update(float DeltaTime) override;
	virtual void Exit() override;

private:
	UPROPERTY()
	UInventoryComponent* Inventory;
	UPROPERTY()
	UHealthComponent* HealthComp;
	UPROPERTY()
	UStaminaComponent* StaminaComp;
	
	bool TryGrabItem();
	void HandleFullInventory();
	bool FindNextKnownItem();
};