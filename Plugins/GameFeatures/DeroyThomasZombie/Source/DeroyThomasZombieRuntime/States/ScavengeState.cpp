#include "ScavengeState.h"
#include "../SurvivorFSM.h"
#include "ExploreState.h"
#include "Survivor/SurvivorPawn.h"
#include "Common/InventoryComponent.h"
#include "Common/HealthComponent.h"
#include "Common/StaminaComponent.h"
#include "Items/BaseItem.h"

void UScavengeState::Enter(USurvivorFSM* FSM)
{
    Super::Enter(FSM);
    GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple, TEXT("State Changed To: Scavenge"));

    if (ContextFSM && ContextFSM->SurvivorPawn)
    {
        // Get components
        Inventory = ContextFSM->SurvivorPawn->GetComponentByClass<UInventoryComponent>();
        HealthComp = ContextFSM->SurvivorPawn->GetComponentByClass<UHealthComponent>();
        StaminaComp = ContextFSM->SurvivorPawn->GetComponentByClass<UStaminaComponent>();
        
        ContextFSM->CurrentPath.Empty(); 
    }
}

void UScavengeState::Update(float DeltaTime)
{
    Super::Update(DeltaTime);

    // Return to explore if wrong data
    if (!ContextFSM || !ContextFSM->SurvivorPawn || !ContextFSM->TargetItem || !Inventory)
    {
        ContextFSM->ChangeState(UExploreState::StaticClass());

        return;
    }
    
    // Check if item within range
    float DistToItem = FVector::Distance(ContextFSM->SurvivorPawn->GetActorLocation(), ContextFSM->TargetItem->GetActorLocation());

    if (DistToItem <= Inventory->GetPickupRange())
    {
        bool bGrabbedSuccessfully = false;

        // Find an empty inventory slot
        for (int i = 0; i < Inventory->GetInventoryCapacity(); ++i)
        {
            if (Inventory->GetInventory()[i] == nullptr)
            {
                if (Inventory->GrabItem(i, ContextFSM->TargetItem))
                {
                    bGrabbedSuccessfully = true;

                    // Item successfully picked up
                    GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("Grabbed Item!"));

                    ContextFSM->KnownItems.Remove(ContextFSM->TargetItem);
                    ContextFSM->TargetItem = nullptr;
                    
                    // Continue if inventory has space
                    if (Inventory->GetInventory().Contains(nullptr))
                    {
                        ABaseItem* ClosestNextItem = nullptr;
                        float ClosestDist = FLT_MAX;

                        // Remove invalid item references
                        for (int j = ContextFSM->KnownItems.Num() - 1; j >= 0; --j)
                        {
                            if (!IsValid(ContextFSM->KnownItems[j]))
                            {
                                ContextFSM->KnownItems.RemoveAt(j);
                            }
                        }

                        // Find closest known item
                        for (ABaseItem* KnownItem : ContextFSM->KnownItems)
                        {
                            float Dist = FVector::Distance(ContextFSM->SurvivorPawn->GetActorLocation(), KnownItem->GetActorLocation());

                            if (Dist < ClosestDist)
                            {
                                ClosestDist = Dist;
                                ClosestNextItem = KnownItem;
                            }
                        }

                        // Set next target
                        if (ClosestNextItem)
                        {
                            ContextFSM->TargetItem = ClosestNextItem;
                            ContextFSM->CurrentPath.Empty(); 

                            return; 
                        }
                    }
                    
                    // Return to explore
                    ContextFSM->ChangeState(UExploreState::StaticClass());

                    return;
                }
            }
        }
        
        // Handle full inventory
        if (!bGrabbedSuccessfully)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Inventory is FULL! Leaving item."));
            
            // Forget item target
            ContextFSM->KnownItems.Remove(ContextFSM->TargetItem);
            ContextFSM->TargetItem = nullptr;
            
            // Return to explore
            ContextFSM->ChangeState(UExploreState::StaticClass());
        }

        return;
    }
    // Generate path to item
    if (ContextFSM->CurrentPath.IsEmpty() || ContextFSM->CurrentPathIndex >= ContextFSM->CurrentPath.Num())
    {
        ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(ContextFSM->TargetItem->GetActorLocation());
        ContextFSM->CurrentPathIndex = 0;
    }
    else
    {
        // Follow current path
        ContextFSM->MoveAlongPath(DeltaTime);
    }
}

void UScavengeState::Exit()
{
    Super::Exit();
    if (ContextFSM)
    {
        ContextFSM->CurrentPath.Empty();
        ContextFSM->TargetItem = nullptr;
    }
}