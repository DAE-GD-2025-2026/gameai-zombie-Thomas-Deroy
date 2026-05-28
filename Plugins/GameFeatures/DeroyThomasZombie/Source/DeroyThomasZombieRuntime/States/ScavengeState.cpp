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
    
    // Check item in pickup range
    float DistToItem = FVector::Distance(ContextFSM->SurvivorPawn->GetActorLocation(), ContextFSM->TargetItem->GetActorLocation());

    if (DistToItem <= Inventory->GetPickupRange())
    {
        // Find empty inventory slot
        for (int i = 0; i < Inventory->GetInventoryCapacity(); ++i)
        {
            if (Inventory->GetInventory()[i] == nullptr)
            {
                if (Inventory->GrabItem(i, ContextFSM->TargetItem))
                {
                    // Item picked up
                    GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("Grabbed Item!"));

                    ContextFSM->KnownItems.Remove(ContextFSM->TargetItem);
                    ContextFSM->TargetItem = nullptr;
                    
                    EvaluateInventory();
                    
                    // Check if there is space
                    if (Inventory->GetInventory().Contains(nullptr))
                    {
                        ABaseItem* ClosestNextItem = nullptr;
                        float ClosestDist = FLT_MAX;

                        // Cleanup
                        for (int j = ContextFSM->KnownItems.Num() - 1; j >= 0; --j)
                        {
                            if (!IsValid(ContextFSM->KnownItems[j]))
                            {
                                ContextFSM->KnownItems.RemoveAt(j);
                            }
                        }

                        // Find closest remaining item we know about
                        for (ABaseItem* KnownItem : ContextFSM->KnownItems)
                        {
                            float Dist = FVector::Distance(ContextFSM->SurvivorPawn->GetActorLocation(), KnownItem->GetActorLocation());
                            if (Dist < ClosestDist)
                            {
                                ClosestDist = Dist;
                                ClosestNextItem = KnownItem;
                            }
                        }

                        // If other items, go there
                        if (ClosestNextItem)
                        {
                            ContextFSM->TargetItem = ClosestNextItem;
                            ContextFSM->CurrentPath.Empty(); // New path
                            return; 
                        }
                    }
                    
                    ContextFSM->ChangeState(UExploreState::StaticClass());

                    return;
                }
            }
        }
        
        ContextFSM->ChangeState(UExploreState::StaticClass());

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

void UScavengeState::EvaluateInventory()
{
    if (!Inventory || !HealthComp || !StaminaComp) return;
    
    // Calculate percentages
    float HealthPercentage = (float)HealthComp->GetHealth() / (float)HealthComp->GetMaxHealth();
    
    float StaminaPercentage = StaminaComp->GetCurrentStamina() / StaminaComp->GetMaxStamina();

    auto const& Items = Inventory->GetInventory();

    for (int i = 0; i < Items.Num(); ++i)
    {
        if (ABaseItem* Item = Items[i])
        {
            // Use medkit if health low
            if (Item->GetItemType() == EItemType::Medkit && HealthPercentage < 0.3f)
            {
                Inventory->UseItem(i);
                Inventory->RemoveItem(i); 

                GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Used Medkit!"));

                break; 
            }
            // Eat food if stamina low
            else if (Item->GetItemType() == EItemType::Food && StaminaPercentage < 0.3f)
            {
                Inventory->UseItem(i);
                Inventory->RemoveItem(i);

                GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("Ate Food!"));

                break;
            }
        }
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