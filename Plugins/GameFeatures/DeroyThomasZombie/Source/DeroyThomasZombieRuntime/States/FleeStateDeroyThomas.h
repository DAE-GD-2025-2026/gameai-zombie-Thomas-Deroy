#pragma once

#include "CoreMinimal.h"
#include "BaseStateDeroyThomas.h"
#include "FleeStateDeroyThomas.generated.h"

UCLASS()
class DEROYTHOMASZOMBIERUNTIME_API UFleeStateDeroyThomas : public UBaseStateDeroyThomas
{
	GENERATED_BODY()

public:
	virtual void Enter(USurvivorFSMDeroyThomas* FSM) override;
	virtual void Update(float DeltaTime) override;
	virtual void Exit() override;
	
private:
	bool bIsCheckingBehind{false};
	float FleeTime{0.0f};
	float CheckTimer{0.0f};
	
	bool HandleShoulderCheck(float DeltaTime, FVector PawnLoc, AActor* ThreatToEvade, FVector LastKnownThreatLoc);
	void UpdateSafetyAndStamina(float DistToThreat, bool bIsPurge);
	void GenerateEscapeRoute(float DeltaTime, FVector PawnLoc, AActor* ThreatToEvade, bool bIsPurge, float DistToThreat);
};