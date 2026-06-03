#pragma once

#include "CoreMinimal.h"
#include "BaseStateDeroyThomas.h"
#include "ReturnStateDeroyThomas.generated.h"

UCLASS()
class DEROYTHOMASZOMBIERUNTIME_API UReturnStateDeroyThomas : public UBaseStateDeroyThomas
{
	GENERATED_BODY()

public:
	virtual void Enter(class USurvivorFSMDeroyThomas* FSM) override;
	virtual void Update(float DeltaTime) override;
	
private:
	bool TryScavengeKnownItems();
	void PathToClosestVisitedHouse();
	void ResetLocalTownCluster();
};