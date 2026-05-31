// Fill out your copyright notice in the Description page of Project Settings.


#include "StudentPerceptor.h"
#include "AIController.h"
#include "SurvivorFSM.h"
#include "Zombies/BaseZombie.h"
#include "Items/BaseItem.h"
#include "Common/InventoryComponent.h"
#include "States/ScavengeState.h"
#include "Village/House/House.h"
#include "PurgeZones/PurgeZone.h"

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
	if (USurvivorFSM* FSM = GetFSM())
	{
		// Feel Damage
		if (Stimulus.Type == UAISense::GetSenseID<UAISense_Damage>())
		{
			if (Stimulus.WasSuccessfullySensed())
			{
				FSM->OnDamageSensed(Stimulus.StimulusLocation);
			}
			return;
		}
		
		// Detect zombie threats
		if (ABaseZombie* Zombie = Cast<ABaseZombie>(Actor))
		{
			if (Stimulus.WasSuccessfullySensed())
			{
				// Remember zombie
				FSM->KnownZombies.AddUnique(Zombie);
				
				// If runner, focus that thing
				if (Zombie->GetClass()->GetName().Contains("Runner")) 
				{
					FSM->CurrentThreat = Zombie; 
				}
				
				// Act
				FSM->OnZombieSpotted(Zombie);
			}
			else
			{
				// Forget zombie
				FSM->KnownZombies.Remove(Zombie);
			}
			return;
		}
		
		// Detect purgezone
		if (APurgeZone* PurgeZone = Cast<APurgeZone>(Actor))
		{
			if (Stimulus.WasSuccessfullySensed())
			{
				FSM->OnPurgeZoneSpotted(PurgeZone);
			}
			else
			{
				FSM->OnPurgeZoneLost(PurgeZone);
			}
			return;
		}

		// Detect items
		if (ABaseItem* Item = Cast<ABaseItem>(Actor))
		{
			if (Stimulus.WasSuccessfullySensed() && !FSM->KnownItems.Contains(Item))
			{
				FSM->KnownItems.Add(Item);

				// Scavenging if inventory has space and no threat
				UInventoryComponent* Inventory = FSM->SurvivorPawn->GetComponentByClass<UInventoryComponent>();
				if (Inventory && Inventory->GetInventory().Contains(nullptr) && !FSM->CurrentThreat && !FSM->TargetItem)
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