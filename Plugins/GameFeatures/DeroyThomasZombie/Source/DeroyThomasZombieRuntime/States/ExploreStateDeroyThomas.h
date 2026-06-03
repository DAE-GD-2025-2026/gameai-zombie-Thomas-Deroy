#pragma once

#include "CoreMinimal.h"
#include "BaseStateDeroyThomas.h" 
#include "ExploreStateDeroyThomas.generated.h"

UCLASS()
class DEROYTHOMASZOMBIERUNTIME_API UExploreStateDeroyThomas : public UBaseStateDeroyThomas
{
	GENERATED_BODY()

public:
	virtual void Enter(USurvivorFSMDeroyThomas* FSM) override;
	virtual void Update(float DeltaTime) override;
	virtual void Exit() override;
	
private:
	int SearchWaypoints{0};
	
	float ParanoiaTimer{0.0f};
	float TimeUntilNextCheck{10.0f};
	bool bIsParanoiaChecking{false};
	float ParanoiaCheckDuration{0.0f};
	float ParanoiaAngle{180.0f};
	FRotator CurrentParanoiaRot;
	
	bool HandleStartSpin(float DeltaTime);
	bool CheckSuppliesAndReturn(float DeltaTime);
	bool HandleHouseSearching();
	bool CheckDesperationMemory();
	void GenerateExplorationPath();
	void HandleParanoia(float DeltaTime);
};