#pragma once

#include "CoreMinimal.h"
#include "BaseState.h"
#include "HideState.generated.h"

UCLASS()
class DEROYTHOMASZOMBIERUNTIME_API UHideState : public UBaseState
{
	GENERATED_BODY()

public:
	virtual void Enter(USurvivorFSM* FSM) override;
	virtual void Update(float DeltaTime) override;
	
private:
	void PathToClosestAvailableHouse();
	void EvaluateThreatDistance();
};