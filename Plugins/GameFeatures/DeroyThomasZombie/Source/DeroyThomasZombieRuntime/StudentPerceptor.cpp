// Fill out your copyright notice in the Description page of Project Settings.


#include "StudentPerceptor.h"
#include "AIController.h"
#include "SurvivorFSM.h"
#include "Zombies/BaseZombie.h"
#include "Items/BaseItem.h"
#include "Common/InventoryComponent.h"
#include "States/ScavengeState.h"
#include "Village/House/House.h"


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
	// Ignore failed perception events
	if (!Stimulus.WasSuccessfullySensed()) return;

	if (USurvivorFSM* FSM = GetFSM())
	{
		// Detect zombie threats
		if (ABaseZombie* Zombie = Cast<ABaseZombie>(Actor))
		{
			FSM->OnZombieSpotted(Zombie);
			return;
		}
		
		// Detect items
		if (ABaseItem* Item = Cast<ABaseItem>(Actor))
		{
			// Ignore garbage items
			if (Item->GetItemType() == EItemType::Garbage) return;
			
			if (!FSM->KnownItems.Contains(Item))
			{
				FSM->KnownItems.Add(Item);
				
				UInventoryComponent* Inventory = FSM->SurvivorPawn->GetComponentByClass<UInventoryComponent>();

				// Scavenging if inventory has space and no threat
				if (Inventory && Inventory->GetInventory().Contains(nullptr) && !FSM->CurrentThreat)
				{
					FSM->TargetItem = Item;
					FSM->ChangeState(UScavengeState::StaticClass());
				}
			}
		}
		
		// Detect houses
		if (AHouse* House = Cast<AHouse>(Actor))
		{
			if (!FSM->KnownHouses.Contains(House) && !FSM->VisitedHouses.Contains(House))
			{
				FSM->KnownHouses.Add(House);

				GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Blue, TEXT("Spotted a House!"));
			}

			return;
		}
	}
}