#include "ReturnStateDeroyThomas.h"

#include "ExploreStateDeroyThomas.h"
#include "../SurvivorFSMDeroyThomas.h"
#include "ScavengeStateDeroyThomas.h"
#include "Common/InventoryComponent.h"
#include "Survivor/SurvivorPawn.h"
#include "Village/House/House.h"

void UReturnStateDeroyThomas::Enter(USurvivorFSMDeroyThomas* FSM)
{
    Super::Enter(FSM);

    GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Orange, TEXT("State Changed To: Return (Need Supplies!)"));

    if (!ContextFSM || !ContextFSM->SurvivorPawn) return;
    
    ContextFSM->SurvivorPawn->StopRunning();
    
    // First known items
    if (TryScavengeKnownItems()) return;

    // Then visited houses
    PathToClosestVisitedHouse();
}

void UReturnStateDeroyThomas::Update(float DeltaTime)
{
    Super::Update(DeltaTime);

    if (!ContextFSM || !ContextFSM->SurvivorPawn) return;

    // Follow path if still traveling
    if (ContextFSM->CurrentPathIndex < ContextFSM->CurrentPath.Num())
    {
        ContextFSM->MoveAlongPath(DeltaTime);
    }
    else
    {
        ResetLocalTownCluster();
    }
}

bool UReturnStateDeroyThomas::TryScavengeKnownItems()
{
    UInventoryComponent* Inventory = ContextFSM->SurvivorPawn->GetComponentByClass<UInventoryComponent>();

    int EmptySlots = 0;

    if (Inventory)
    {
        for (auto const& Item : Inventory->GetInventory())
        {
            if (!Item) EmptySlots++;
        }
    }

    // If we have space for new items
    if (EmptySlots > 0 && ContextFSM->KnownItems.Num() > 0)
    {
        AActor* BestItem = nullptr;
        float ClosestDist = FLT_MAX;

        // Find closest valid item
        for (int i = ContextFSM->KnownItems.Num() - 1; i >= 0; --i)
        {
            AActor* ItemActor = ContextFSM->KnownItems[i];

            if (IsValid(ItemActor))
            {
                float Dist = FVector::Distance(
                    ContextFSM->SurvivorPawn->GetActorLocation(),
                    ItemActor->GetActorLocation()
                );

                if (Dist < ClosestDist)
                {
                    ClosestDist = Dist;
                    BestItem = ItemActor;
                }
            }
            else
            {
                ContextFSM->KnownItems.RemoveAt(i);
            }
        }

        if (BestItem)
        {
            ContextFSM->TargetItem = Cast<ABaseItem>(BestItem);

            ContextFSM->ChangeState(UScavengeStateDeroyThomas::StaticClass());

            return true;
        }
    }

    return false;
}

void UReturnStateDeroyThomas::PathToClosestVisitedHouse()
{
    AHouse* ClosestHouse = nullptr;
    float ClosestDist = FLT_MAX;

    for (AHouse* House : ContextFSM->VisitedHouses)
    {
        if (IsValid(House))
        {
            float Dist = FVector::Distance(
                ContextFSM->SurvivorPawn->GetActorLocation(),
                House->GetActorLocation()
            );

            if (Dist < ClosestDist)
            {
                ClosestDist = Dist;
                ClosestHouse = House;
            }
        }
    }

    if (ClosestHouse)
    {
        ContextFSM->TargetHouse = ClosestHouse;

        ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(
            ClosestHouse->GetBounds().Origin
        );

        ContextFSM->CurrentPathIndex = 0;
        
        // If path is invalid
        if (ContextFSM->CurrentPath.IsEmpty())
        {
            ContextFSM->VisitedHouses.Remove(ClosestHouse);

            ContextFSM->TargetHouse = nullptr;

            ContextFSM->ChangeState(UExploreStateDeroyThomas::StaticClass());
        }
    }
    else
    {
        // No known houses -> just explore
        ContextFSM->ChangeState(UExploreStateDeroyThomas::StaticClass());
    }
}

void UReturnStateDeroyThomas::ResetLocalTownCluster()
{
    TArray<AHouse*> LocalTownHouses;

    float TownRadius = 3000.0f;

    for (AHouse* House : ContextFSM->VisitedHouses)
    {
        if (IsValid(House))
        {
            float DistToTownCenter = FVector::Distance(
                House->GetActorLocation(),
                ContextFSM->TargetHouse->GetActorLocation()
            );

            if (DistToTownCenter <= TownRadius)
            {
                LocalTownHouses.Add(House);
            }
        }
    }

    for (AHouse* House : LocalTownHouses)
    {
        ContextFSM->VisitedHouses.Remove(House);
        ContextFSM->KnownHouses.Add(House);
    }
    
    ContextFSM->TargetHouse = nullptr;

    ContextFSM->TimeSinceLastTown = 0.0f;

    ContextFSM->ResumePreviousState();
}