#pragma once

#include "CoreMinimal.h"
#include "BaseState.h"
#include "FleeState.generated.h"

UCLASS()
class DEROYTHOMASZOMBIERUNTIME_API UFleeState : public UBaseState
{
	GENERATED_BODY()

public:
	virtual void Enter(USurvivorFSM* FSM) override;
	virtual void Update(float DeltaTime) override;
	virtual void Exit() override;
	
private:
	bool bIsCheckingBehind{false};
	float FleeTime{0.0f};
	float CheckTimer{0.0f};
};