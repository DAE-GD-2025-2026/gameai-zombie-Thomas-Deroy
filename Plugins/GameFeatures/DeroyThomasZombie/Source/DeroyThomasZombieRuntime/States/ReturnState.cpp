#include "ReturnState.h"
#include "../SurvivorFSM.h"
#include "ExploreState.h"
#include "ScavengeState.h"
#include "Common/InventoryComponent.h"
#include "Survivor/SurvivorPawn.h"
#include "Village/House/House.h"

void UReturnState::Enter(USurvivorFSM* FSM)
{
    Super::Enter(FSM);
    
    GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Orange, TEXT("State Changed To: Return (Need Supplies!)"));

    if (!ContextFSM) return;
    
    ContextFSM->SurvivorPawn->StopRunning();
    
    UInventoryComponent* Inventory = ContextFSM->SurvivorPawn->GetComponentByClass<UInventoryComponent>();
    int EmptySlots = 0;
    if (Inventory) 
    {
        for (auto const& Item : Inventory->GetInventory()) { if (!Item) EmptySlots++; }
    }

    if (EmptySlots > 0 && ContextFSM->KnownItems.Num() > 0)
    {
        AActor* BestItem = nullptr;
        float ClosestDist = FLT_MAX;

        // Find the closest known item
        for (int i = ContextFSM->KnownItems.Num() - 1; i >= 0; --i)
        {
            AActor* ItemActor = ContextFSM->KnownItems[i];
            if (IsValid(ItemActor))
            {
                float Dist = FVector::Distance(ContextFSM->SurvivorPawn->GetActorLocation(), ItemActor->GetActorLocation());
                if (Dist < ClosestDist)
                {
                    ClosestDist = Dist;
                    BestItem = ItemActor;
                }
            }
            else
            {
                ContextFSM->KnownItems.RemoveAt(i); // Clean up destroyed items
            }
        }

        if (BestItem)
        {
            // Taking the item
            ContextFSM->TargetItem = Cast<ABaseItem>(BestItem);
            
            ContextFSM->ChangeState(UScavengeState::StaticClass());
            return; 
        }
    }
    
    AHouse* ClosestHouse = nullptr;
    float ClosestDist = FLT_MAX;

    for (AHouse* House : ContextFSM->VisitedHouses)
    {
        if (IsValid(House))
        {
            float Dist = FVector::Distance(ContextFSM->SurvivorPawn->GetActorLocation(), House->GetActorLocation());

            if (Dist < ClosestDist)
            {
                ClosestDist = Dist;
                ClosestHouse = House;
            }
        }
    }

    if (ClosestHouse)
    {
        // Create path to closest known house
        ContextFSM->TargetHouse = ClosestHouse;

        ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(ClosestHouse->GetBounds().Origin);
        ContextFSM->CurrentPathIndex = 0;
        
        // if no path found
        if (ContextFSM->CurrentPath.IsEmpty())
        {
            ContextFSM->VisitedHouses.Remove(ClosestHouse);
            ContextFSM->TargetHouse = nullptr;
            ContextFSM->ChangeState(UExploreState::StaticClass());
        }
    }
    else
    {
        // No visited houses available
        ContextFSM->ChangeState(UExploreState::StaticClass());
    }
}

void UReturnState::Update(float DeltaTime)
{
    Super::Update(DeltaTime);
    
    if (!ContextFSM || !ContextFSM->SurvivorPawn) return;

    // Follow path to destination house
    if (ContextFSM->CurrentPathIndex < ContextFSM->CurrentPath.Num())
    {
        ContextFSM->MoveAlongPath(DeltaTime);
    }
    else
    {
        TArray<AHouse*> LocalTownHouses;
        
        float TownRadius = 3000.0f;

        // Find nearby visited houses
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

        // Move nearby houses back into exploration memory
        for (AHouse* House : LocalTownHouses)
        {
            ContextFSM->VisitedHouses.Remove(House);
            ContextFSM->KnownHouses.Add(House);
        }
        
        ContextFSM->TargetHouse = nullptr;
        
        // Reset return timer
        ContextFSM->TimeSinceLastTown = 0.0f;
        
        ContextFSM->ResumePreviousState();
    }
}