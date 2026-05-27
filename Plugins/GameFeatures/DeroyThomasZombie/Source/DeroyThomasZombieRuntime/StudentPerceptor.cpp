// Fill out your copyright notice in the Description page of Project Settings.


#include "StudentPerceptor.h"
#include "AIController.h"
#include "SurvivorFSM.h"
#include "Zombies/BaseZombie.h"


UStudentPerceptor::UStudentPerceptor()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UStudentPerceptor::BeginPlay()
{
	Super::BeginPlay();
	
	if (auto PerceptionComp = GetOwner()->GetComponentByClass<UAIPerceptionComponent>())
	{
		PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &UStudentPerceptor::OnPerceptionUpdated);
	}
}

USurvivorFSM* UStudentPerceptor::GetFSM() const
{
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (AAIController* Controller = Cast<AAIController>(OwnerPawn->GetController()))
		{
			return Controller->GetComponentByClass<USurvivorFSM>();
		}
	}
	return nullptr;
}

void UStudentPerceptor::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Stimulus.WasSuccessfullySensed()) return;
	
	if (ABaseZombie* Zombie = Cast<ABaseZombie>(Actor))
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("Run broski you're cooked!"));
		
		if (USurvivorFSM* FSM = GetFSM())
		{
			FSM->OnZombieSpotted(Zombie);
		}
	}
}
