#pragma once

#include "CoreMinimal.h"
#include "BaseState.h"
#include "CombatState.generated.h"

UCLASS()
class DEROYTHOMASZOMBIERUNTIME_API UCombatState : public UBaseState
{
	GENERATED_BODY()

public:
	virtual void Enter(USurvivorFSM* FSM) override;
	virtual void Update(float DeltaTime) override;

private:
	float TimeSinceLastShot{0.0f};
	float FireRate{1.0f}; 
	
	void HandleAimingAndMovement(float DeltaTime, FVector ThreatDir);
	void HandleShooting(FVector ThreatDir);
};