#pragma once

#include "CoreMinimal.h"
#include "BaseState.h" 
#include "ExploreState.generated.h"

UCLASS()
class DEROYTHOMASZOMBIERUNTIME_API UExploreState : public UBaseState
{
	GENERATED_BODY()

public:
	virtual void Enter(USurvivorFSM* FSM) override;
	virtual void Update(float DeltaTime) override;
	virtual void Exit() override;
	
private:
	int SearchWaypoints{0};
	
	float ParanoiaTimer{0.0f};
	float TimeUntilNextCheck{10.0f};
	bool bIsParanoiaChecking{false};
	float ParanoiaCheckDuration{0.0f};
	float ParanoiaAngle{180.0f};
};