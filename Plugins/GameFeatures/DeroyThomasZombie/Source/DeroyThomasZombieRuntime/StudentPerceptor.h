// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISense_Damage.h"
#include "StudentPerceptor.generated.h"

class USurvivorFSM;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEROYTHOMASZOMBIERUNTIME_API UStudentPerceptor : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UStudentPerceptor();
	
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
	
private:
	USurvivorFSM* GetFSM() const;
	
	void HandleDamageSensed(USurvivorFSM* FSM, FAIStimulus& Stimulus);
	void HandleZombieSensed(USurvivorFSM* FSM, AActor* Actor, FAIStimulus& Stimulus);
	void HandlePurgeZoneSensed(USurvivorFSM* FSM, AActor* Actor, FAIStimulus& Stimulus);
	void HandleItemSensed(USurvivorFSM* FSM, AActor* Actor, FAIStimulus& Stimulus);
	void HandleHouseSensed(USurvivorFSM* FSM, AActor* Actor);
};
