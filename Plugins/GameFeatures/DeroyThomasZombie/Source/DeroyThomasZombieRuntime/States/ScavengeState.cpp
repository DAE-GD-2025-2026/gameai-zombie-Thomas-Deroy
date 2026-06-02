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
        Inventory = ContextFSM->SurvivorPawn->GetComponentByClass<UInventoryComponent>();
        HealthComp = ContextFSM->SurvivorPawn->GetComponentByClass<UHealthComponent>();
        StaminaComp = ContextFSM->SurvivorPawn->GetComponentByClass<UStaminaComponent>();
        
        // Stop current movement
        ContextFSM->CurrentPath.Empty(); 
    }
}

void UScavengeState::Update(float DeltaTime)
{
    Super::Update(DeltaTime);
    
    if (!ContextFSM || !ContextFSM->SurvivorPawn || !ContextFSM->TargetItem || !Inventory)
    {
        ContextFSM->ResumePreviousState();
        return;
    }
    
    float DistToItem = FVector::Distance(
        ContextFSM->SurvivorPawn->GetActorLocation(),
        ContextFSM->TargetItem->GetActorLocation()
    );

    // Check pickup range
    if (DistToItem <= Inventory->GetPickupRange())
    {
        if (TryGrabItem())
        {
            // After taking one item, try to get another
            if (!FindNextKnownItem()) 
            {
                ContextFSM->ResumePreviousState();
            }
        }
        else
        {
            HandleFullInventory();
        }

        return;
    }

    // Path to target item
    if (ContextFSM->CurrentPath.IsEmpty() || ContextFSM->CurrentPathIndex >= ContextFSM->CurrentPath.Num())
    {
        ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(
            ContextFSM->TargetItem->GetActorLocation()
        );

        ContextFSM->CurrentPathIndex = 0;
    }
    else
    {
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

bool UScavengeState::TryGrabItem()
{
    // Try all inventory slots
    for (int i = 0; i < Inventory->GetInventoryCapacity(); ++i)
    {
        if (Inventory->GetInventory()[i] == nullptr)
        {
            if (Inventory->GrabItem(i, ContextFSM->TargetItem))
            {
                GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("Grabbed Item!"));

                ContextFSM->KnownItems.Remove(ContextFSM->TargetItem);
                ContextFSM->TargetItem = nullptr;

                return true;
            }
        }
    }

    // No empty slot
    return false;
}

void UScavengeState::HandleFullInventory()
{
    bool bSwapped = false;

    EItemType TargetType = ContextFSM->TargetItem->GetItemType();

    // Swaps for useful item types
    if (TargetType == EItemType::Food || TargetType == EItemType::Shotgun ||
        TargetType == EItemType::Pistol || TargetType == EItemType::Medkit)
    {
        auto const& Items = Inventory->GetInventory();

        for (int i = 0; i < Items.Num(); ++i)
        {
            if (Items[i] != nullptr)
            {
                // Replace food if stamina is already fine
                if (Items[i]->GetItemType() == EItemType::Food &&
                    StaminaComp &&
                    (StaminaComp->GetCurrentStamina() / StaminaComp->GetMaxStamina()) > 0.5f)
                {
                    GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("Dropped Food for a better item!"));

                    Inventory->RemoveItem(i);
                    Inventory->GrabItem(i, ContextFSM->TargetItem);

                    bSwapped = true;
                    break;
                }
            }
        }
    }

    // Debug
    if (!bSwapped)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Inventory FULL! Leaving it."));
    }
    else
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("Successfully Swapped Item!"));
    }

    ContextFSM->KnownItems.Remove(ContextFSM->TargetItem);
    ContextFSM->TargetItem = nullptr;

    ContextFSM->ResumePreviousState();
}

bool UScavengeState::FindNextKnownItem()
{
    // No space left
    if (!Inventory->GetInventory().Contains(nullptr)) return false;

    ABaseItem* ClosestNextItem = nullptr;
    float ClosestDist = FLT_MAX;

    // Clean invalid items first
    for (int j = ContextFSM->KnownItems.Num() - 1; j >= 0; --j)
    {
        if (!IsValid(ContextFSM->KnownItems[j]))
        {
            ContextFSM->KnownItems.RemoveAt(j);
        }
    }

    // Find nearest known item
    for (ABaseItem* KnownItem : ContextFSM->KnownItems)
    {
        float Dist = FVector::Distance(
            ContextFSM->SurvivorPawn->GetActorLocation(),
            KnownItem->GetActorLocation()
        );

        if (Dist < ClosestDist)
        {
            ClosestDist = Dist;
            ClosestNextItem = KnownItem;
        }
    }

    if (ClosestNextItem)
    {
        ContextFSM->TargetItem = ClosestNextItem;
        ContextFSM->CurrentPath.Empty(); 
        return true; 
    }

    return false;
}