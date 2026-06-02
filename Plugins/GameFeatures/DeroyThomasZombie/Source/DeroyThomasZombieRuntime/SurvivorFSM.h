#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "States/BaseState.h"
#include "Items/BaseItem.h"
#include "Village/House/House.h"
#include "SurvivorFSM.generated.h"

class ASurvivorPawn;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DEROYTHOMASZOMBIERUNTIME_API USurvivorFSM : public UActorComponent
{
	GENERATED_BODY()

public: 
	USurvivorFSM();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void OnZombieSpotted(AActor* Zombie);
	void ChangeState(TSubclassOf<UBaseState> NewStateClass);
	void ResumePreviousState();
	void MoveAlongPath(float DeltaTime);
	
	UPROPERTY()
	ASurvivorPawn* SurvivorPawn;

	TArray<FVector> CurrentPath;
	int32 CurrentPathIndex{0};
	UPROPERTY()
	FVector CurrentVelocity{FVector::ZeroVector};
	
	UPROPERTY()
	AActor* CurrentThreat;
	
	UPROPERTY()
	TArray<ABaseItem*> KnownItems;
	AActor* GetClosestKnownItem(EItemType DesiredType);
	UPROPERTY()
	TArray<AHouse*> KnownHouses;
	
	UPROPERTY()
	ABaseItem* TargetItem;
	UPROPERTY()
	AHouse* TargetHouse;
	
	UPROPERTY()
	TArray<AHouse*> VisitedHouses;
	
	UPROPERTY()
	TArray<AActor*> KnownZombies;
	
	bool GetBestWeapon(float TargetDistance, int& OutSlotIndex);
	bool HasUsableWeapon(int& OutSlotIndex);
	
	UPROPERTY()
	AActor* ActivePurgeZone;

	void OnPurgeZoneSpotted(AActor* PurgeZone);
	void OnPurgeZoneLost(AActor* PurgeZone);
	
	UPROPERTY()
	bool bHasDoneStartSpin{false};
	float SpinTimer{0.0f};
	
	void EvaluateInventory();
	
	void OnDamageSensed(FVector DamageLocation);
	int LastKnownHealth{-1};	
	bool bIsReflexSpinning{false};
	FVector ReflexTargetLocation;
	
	UPROPERTY()
	float TimeSinceLastTown{0.0f};
	
	UPROPERTY()
	TSubclassOf<UBaseState> PreviousStateClass;
	
	UPROPERTY()
	FVector LastFrameLocation{FVector::ZeroVector};
    
	UPROPERTY()
	float StuckTimer{0.0f};
	
	// Debug
	UPROPERTY()
	bool bShowDebug{true};
	UPROPERTY()
	TArray<AHouse*> AllLevelHouses;
	UPROPERTY()
	TMap<AHouse*, int32> DiscoveredHouseIDs;
	int32 TotalHousesDiscovered{0};
	UPROPERTY()
	TArray<AActor*> AllLevelItems;
	
protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	UBaseState* CurrentState;
};