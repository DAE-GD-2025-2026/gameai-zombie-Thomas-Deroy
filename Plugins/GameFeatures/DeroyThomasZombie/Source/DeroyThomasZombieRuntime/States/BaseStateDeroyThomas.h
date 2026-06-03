#pragma once

#include "CoreMinimal.h"
#include "BaseStateDeroyThomas.generated.h"

class USurvivorFSMDeroyThomas;

UCLASS(Abstract)
class DEROYTHOMASZOMBIERUNTIME_API UBaseStateDeroyThomas : public UObject
{
	GENERATED_BODY()

public:
	virtual void Enter(USurvivorFSMDeroyThomas* FSM);
	virtual void Update(float DeltaTime);
	virtual void Exit();

protected:
	UPROPERTY()
	USurvivorFSMDeroyThomas* ContextFSM;
};