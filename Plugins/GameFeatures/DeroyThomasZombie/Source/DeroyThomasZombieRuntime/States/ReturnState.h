#pragma once

#include "CoreMinimal.h"
#include "BaseState.h"
#include "ReturnState.generated.h"

UCLASS()
class DEROYTHOMASZOMBIERUNTIME_API UReturnState : public UBaseState
{
	GENERATED_BODY()

public:
	virtual void Enter(class USurvivorFSM* FSM) override;
	virtual void Update(float DeltaTime) override;
};