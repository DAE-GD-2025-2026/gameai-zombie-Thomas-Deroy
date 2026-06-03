#pragma once

#include "CoreMinimal.h"
#include "BaseStateDeroyThomas.h"
#include "HideStateDeroyThomas.generated.h"

UCLASS()
class DEROYTHOMASZOMBIERUNTIME_API UHideStateDeroyThomas : public UBaseStateDeroyThomas
{
	GENERATED_BODY()

public:
	virtual void Enter(USurvivorFSMDeroyThomas* FSM) override;
	virtual void Update(float DeltaTime) override;
	
private:
	void PathToClosestAvailableHouse();
	void EvaluateThreatDistance();
};