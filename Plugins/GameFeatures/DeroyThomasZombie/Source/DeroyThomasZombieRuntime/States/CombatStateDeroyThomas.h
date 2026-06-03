#pragma once

#include "CoreMinimal.h"
#include "BaseStateDeroyThomas.h"
#include "CombatStateDeroyThomas.generated.h"

UCLASS()
class DEROYTHOMASZOMBIERUNTIME_API UCombatStateDeroyThomas : public UBaseStateDeroyThomas
{
	GENERATED_BODY()

public:
	virtual void Enter(USurvivorFSMDeroyThomas* FSM) override;
	virtual void Update(float DeltaTime) override;

private:
	float TimeSinceLastShot{0.0f};
	float FireRate{1.0f}; 
	
	void HandleAimingAndMovement(float DeltaTime, FVector ThreatDir);
	void HandleShooting(FVector ThreatDir);
};