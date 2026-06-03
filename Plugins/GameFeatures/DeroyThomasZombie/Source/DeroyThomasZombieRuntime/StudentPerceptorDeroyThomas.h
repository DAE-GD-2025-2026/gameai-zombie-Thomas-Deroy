// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISense_Damage.h"
#include "StudentPerceptorDeroyThomas.generated.h"

class USurvivorFSMDeroyThomas;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEROYTHOMASZOMBIERUNTIME_API UStudentPerceptorDeroyThomas : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UStudentPerceptorDeroyThomas();
	
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
	
private:
	USurvivorFSMDeroyThomas* GetFSM() const;
	
	void HandleDamageSensed(USurvivorFSMDeroyThomas* FSM, FAIStimulus& Stimulus);
	void HandleZombieSensed(USurvivorFSMDeroyThomas* FSM, AActor* Actor, FAIStimulus& Stimulus);
	void HandlePurgeZoneSensed(USurvivorFSMDeroyThomas* FSM, AActor* Actor, FAIStimulus& Stimulus);
	void HandleItemSensed(USurvivorFSMDeroyThomas* FSM, AActor* Actor, FAIStimulus& Stimulus);
	void HandleHouseSensed(USurvivorFSMDeroyThomas* FSM, AActor* Actor);
};
