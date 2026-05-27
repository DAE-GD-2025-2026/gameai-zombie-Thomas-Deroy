#pragma once

#include "CoreMinimal.h"
#include "BaseState.generated.h"

class USurvivorFSM;

UCLASS(Abstract)
class DEROYTHOMASZOMBIERUNTIME_API UBaseState : public UObject
{
	GENERATED_BODY()

public:
	virtual void Enter(USurvivorFSM* FSM);
	virtual void Update(float DeltaTime);
	virtual void Exit();

protected:
	UPROPERTY()
	USurvivorFSM* ContextFSM;
};