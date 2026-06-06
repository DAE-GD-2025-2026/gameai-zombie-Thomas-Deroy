// Fill out your copyright notice in the Description page of Project Settings.


#include "StudentPerceptorDeroyThomas.h"
#include "AIController.h"
#include "SurvivorFSMDeroyThomas.h"
#include "Zombies/BaseZombie.h"
#include "Items/BaseItem.h"
#include "Common/InventoryComponent.h"
#include "States/ScavengeStateDeroyThomas.h"
#include "Village/House/House.h"
#include "PurgeZones/PurgeZone.h"

UStudentPerceptorDeroyThomas::UStudentPerceptorDeroyThomas()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UStudentPerceptorDeroyThomas::BeginPlay()
{
	Super::BeginPlay();
	
	if (auto PerceptionComp = GetOwner()->GetComponentByClass<UAIPerceptionComponent>())
	{
		PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &UStudentPerceptorDeroyThomas::OnPerceptionUpdated);
	}
}

USurvivorFSMDeroyThomas* UStudentPerceptorDeroyThomas::GetFSM() const
{
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (AAIController* Controller = Cast<AAIController>(OwnerPawn->GetController()))
		{
			return Controller->GetComponentByClass<USurvivorFSMDeroyThomas>();
		}
	}
	return nullptr;
}

void UStudentPerceptorDeroyThomas::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	USurvivorFSMDeroyThomas* FSM = GetFSM();
	if (!FSM) return;
	
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Damage>()) HandleDamageSensed(FSM, Stimulus);
	else if (Actor->IsA(ABaseZombie::StaticClass())) HandleZombieSensed(FSM, Actor, Stimulus);
	else if (Actor->IsA(APurgeZone::StaticClass())) HandlePurgeZoneSensed(FSM, Actor, Stimulus);
	else if (Actor->IsA(ABaseItem::StaticClass())) HandleItemSensed(FSM, Actor, Stimulus);
	else if (Actor->IsA(AHouse::StaticClass())) HandleHouseSensed(FSM, Actor, Stimulus);
}

void UStudentPerceptorDeroyThomas::HandleDamageSensed(USurvivorFSMDeroyThomas* FSM, FAIStimulus& Stimulus)
{
    if (Stimulus.WasSuccessfullySensed())
    {
        FSM->OnDamageSensed(Stimulus.StimulusLocation);
    }
}

void UStudentPerceptorDeroyThomas::HandleZombieSensed(USurvivorFSMDeroyThomas* FSM, AActor* Actor, FAIStimulus& Stimulus)
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

void UStudentPerceptorDeroyThomas::HandlePurgeZoneSensed(USurvivorFSMDeroyThomas* FSM, AActor* Actor, FAIStimulus& Stimulus)
{
    APurgeZone* PurgeZone = Cast<APurgeZone>(Actor);
    if (!PurgeZone) return;

    if (Stimulus.WasSuccessfullySensed()) FSM->OnPurgeZoneSpotted(PurgeZone);
}

void UStudentPerceptorDeroyThomas::HandleItemSensed(USurvivorFSMDeroyThomas* FSM, AActor* Actor, FAIStimulus& Stimulus)
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
            FSM->ChangeState(UScavengeStateDeroyThomas::StaticClass());
        }
    }
}

void UStudentPerceptorDeroyThomas::HandleHouseSensed(USurvivorFSMDeroyThomas* FSM, AActor* Actor, FAIStimulus& Stimulus)
{
	AHouse* House = Cast<AHouse>(Actor);
	if (!House) return;
	
	if (Stimulus.WasSuccessfullySensed() && !FSM->KnownHouses.Contains(House) && !FSM->VisitedHouses.Contains(House))
	{
		FSM->KnownHouses.Add(House);
		FSM->TotalHousesDiscovered++;
		FSM->DiscoveredHouseIDs.Add(House, FSM->TotalHousesDiscovered);

		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Blue, TEXT("Spotted a House!"));
	}
}