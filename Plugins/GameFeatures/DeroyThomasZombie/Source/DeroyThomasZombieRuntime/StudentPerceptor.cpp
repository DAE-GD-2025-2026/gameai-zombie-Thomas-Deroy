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
	USurvivorFSM* FSM = GetFSM();
	if (!FSM) return;
	
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Damage>()) HandleDamageSensed(FSM, Stimulus);
	else if (Actor->IsA(ABaseZombie::StaticClass())) HandleZombieSensed(FSM, Actor, Stimulus);
	else if (Actor->IsA(APurgeZone::StaticClass())) HandlePurgeZoneSensed(FSM, Actor, Stimulus);
	else if (Actor->IsA(ABaseItem::StaticClass())) HandleItemSensed(FSM, Actor, Stimulus);
	else if (Actor->IsA(AHouse::StaticClass())) HandleHouseSensed(FSM, Actor);
}

void UStudentPerceptor::HandleDamageSensed(USurvivorFSM* FSM, FAIStimulus& Stimulus)
{
    if (Stimulus.WasSuccessfullySensed())
    {
        FSM->OnDamageSensed(Stimulus.StimulusLocation);
    }
}

void UStudentPerceptor::HandleZombieSensed(USurvivorFSM* FSM, AActor* Actor, FAIStimulus& Stimulus)
{
    ABaseZombie* Zombie = Cast<ABaseZombie>(Actor);
    if (!Zombie) return;

    if (Stimulus.WasSuccessfullySensed())
    {
        FSM->KnownZombies.AddUnique(Zombie);
        
        if (Zombie->GetClass()->GetName().Contains("Runner")) 
        {
            FSM->CurrentThreat = Zombie; 
        }
        FSM->OnZombieSpotted(Zombie);
    }
    else
    {
        FSM->KnownZombies.Remove(Zombie);
    }
}

void UStudentPerceptor::HandlePurgeZoneSensed(USurvivorFSM* FSM, AActor* Actor, FAIStimulus& Stimulus)
{
    APurgeZone* PurgeZone = Cast<APurgeZone>(Actor);
    if (!PurgeZone) return;

    if (Stimulus.WasSuccessfullySensed()) FSM->OnPurgeZoneSpotted(PurgeZone);
    else FSM->OnPurgeZoneLost(PurgeZone);
}

void UStudentPerceptor::HandleItemSensed(USurvivorFSM* FSM, AActor* Actor, FAIStimulus& Stimulus)
{
    ABaseItem* Item = Cast<ABaseItem>(Actor);
    if (!Item) return;

    if (Stimulus.WasSuccessfullySensed() && !FSM->KnownItems.Contains(Item))
    {
        FSM->KnownItems.Add(Item);

        UInventoryComponent* Inventory = FSM->SurvivorPawn->GetComponentByClass<UInventoryComponent>();
        if (Inventory && Inventory->GetInventory().Contains(nullptr) && !FSM->CurrentThreat && !FSM->TargetItem)
        {
            FSM->TargetItem = Item;
            FSM->ChangeState(UScavengeState::StaticClass());
        }
    }
}

void UStudentPerceptor::HandleHouseSensed(USurvivorFSM* FSM, AActor* Actor)
{
    AHouse* House = Cast<AHouse>(Actor);
    if (!House) return;

    if (!FSM->KnownHouses.Contains(House) && !FSM->VisitedHouses.Contains(House))
    {
        FSM->KnownHouses.Add(House);
        
        FSM->TotalHousesDiscovered++;
        FSM->DiscoveredHouseIDs.Add(House, FSM->TotalHousesDiscovered);

        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Blue, TEXT("Spotted a House!"));
    }
}