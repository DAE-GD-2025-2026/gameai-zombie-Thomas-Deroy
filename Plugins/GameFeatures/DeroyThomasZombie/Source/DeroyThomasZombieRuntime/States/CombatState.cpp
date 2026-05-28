#include "CombatState.h"
#include "../SurvivorFSM.h"
#include "ExploreState.h"
#include "FleeState.h"
#include "Survivor/SurvivorPawn.h"
#include "Common/InventoryComponent.h"
#include "Items/BaseItem.h"

void UCombatState::Enter(USurvivorFSM* FSM)
{
    Super::Enter(FSM);
    GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("State Changed To: Combat"));

    if (ContextFSM && ContextFSM->SurvivorPawn)
    {
        ContextFSM->SurvivorPawn->StopRunning(); // Stop to aim
        ContextFSM->CurrentPath.Empty();
        TimeSinceLastShot = FireRate; // Shoot
    }
}

void UCombatState::Update(float DeltaTime)
{
    Super::Update(DeltaTime);

    // We chill
    if (!IsValid(ContextFSM->CurrentThreat))
    {
        ContextFSM->CurrentThreat = nullptr;
        ContextFSM->ChangeState(UExploreState::StaticClass());
        return;
    }

    // Aim at zombie
    FVector ThreatDir = (ContextFSM->CurrentThreat->GetActorLocation() - ContextFSM->SurvivorPawn->GetActorLocation()).GetSafeNormal();
    ThreatDir.Z = 0.0f; 
    
    FRotator TargetRot = ThreatDir.Rotation();
    FRotator SmoothRot = FMath::RInterpTo(ContextFSM->SurvivorPawn->GetActorRotation(), TargetRot, DeltaTime, 15.0f);
    ContextFSM->SurvivorPawn->SetActorRotation(SmoothRot);

    // Shoot
    TimeSinceLastShot += DeltaTime;
    if (TimeSinceLastShot >= FireRate)
    {
        // Shoot if looking at it
        if (FVector::DotProduct(ContextFSM->SurvivorPawn->GetActorForwardVector(), ThreatDir) > 0.9f)
        {
            int WeaponSlot = -1;
            if (ContextFSM->HasUsableWeapon(WeaponSlot))
            {
                UInventoryComponent* Inventory = ContextFSM->SurvivorPawn->GetComponentByClass<UInventoryComponent>();
                Inventory->UseItem(WeaponSlot);
                TimeSinceLastShot = 0.0f;
                GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, TEXT("BANG!"));

                // No bullets
                if (Inventory->GetInventory()[WeaponSlot]->GetValue() <= 0)
                {
                    Inventory->RemoveItem(WeaponSlot);
                }
            }
            else
            {
                // No bullets
                GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("Out of ammo! Fleeing!"));
                ContextFSM->ChangeState(UFleeState::StaticClass());
            }
        }
    }
}