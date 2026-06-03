#include "CombatStateDeroyThomas.h"
#include "../SurvivorFSMDeroyThomas.h"
#include "ExploreStateDeroyThomas.h"
#include "FleeStateDeroyThomas.h"
#include "Survivor/SurvivorPawn.h"
#include "Common/InventoryComponent.h"
#include "Items/BaseItem.h"

void UCombatStateDeroyThomas::Enter(USurvivorFSMDeroyThomas* FSM)
{
    Super::Enter(FSM);

    GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("State Changed To: Combat"));

    if (ContextFSM && ContextFSM->SurvivorPawn)
    {
        ContextFSM->SurvivorPawn->StopRunning();
        ContextFSM->CurrentPath.Empty();
        TimeSinceLastShot = FireRate; 
    }
}

void UCombatStateDeroyThomas::Update(float DeltaTime)
{
    Super::Update(DeltaTime);

    if (!ContextFSM || !ContextFSM->SurvivorPawn) return;

    // If purge zone then flee
    if (ContextFSM->ActivePurgeZone)
    {
        ContextFSM->ChangeState(UFleeStateDeroyThomas::StaticClass());
        return;
    }

    // Lose target -> return to previous behavior
    if (!IsValid(ContextFSM->CurrentThreat))
    {
        ContextFSM->CurrentThreat = nullptr;
        ContextFSM->ResumePreviousState();
        return;
    }

    FVector ThreatDir = (ContextFSM->CurrentThreat->GetActorLocation() - ContextFSM->SurvivorPawn->GetActorLocation()).GetSafeNormal();
    ThreatDir.Z = 0.0f; 

    HandleAimingAndMovement(DeltaTime, ThreatDir);
    
    TimeSinceLastShot += DeltaTime;

    if (TimeSinceLastShot >= FireRate)
    {
        HandleShooting(ThreatDir);
    }
}

void UCombatStateDeroyThomas::HandleAimingAndMovement(float DeltaTime, FVector ThreatDir)
{
    // Rotate toward target
    FRotator TargetRot = ThreatDir.Rotation();

    FRotator SmoothRot = FMath::RInterpTo(
        ContextFSM->SurvivorPawn->GetActorRotation(),
        TargetRot,
        DeltaTime,
        15.0f
    );

    ContextFSM->SurvivorPawn->SetActorRotation(SmoothRot);
    
    // backward movement while fighting
    ContextFSM->SurvivorPawn->AddMovementInput(-ThreatDir, 0.5f);
}

void UCombatStateDeroyThomas::HandleShooting(FVector ThreatDir)
{
    // Only shoot when roughly aimed at target
    if (FVector::DotProduct(ContextFSM->SurvivorPawn->GetActorForwardVector(), ThreatDir) > 0.9f)
    {
        float ThreatDist = FVector::Distance(
            ContextFSM->SurvivorPawn->GetActorLocation(),
            ContextFSM->CurrentThreat->GetActorLocation()
        );

        // Skip shooting if too far
        if (ThreatDist > 700.0f) return;

        int WeaponSlot = -1;

        if (ContextFSM->GetBestWeapon(ThreatDist, WeaponSlot))
        {
            UInventoryComponent* Inventory = ContextFSM->SurvivorPawn->GetComponentByClass<UInventoryComponent>();

            Inventory->UseItem(WeaponSlot);

            TimeSinceLastShot = 0.0f;

            GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, TEXT("BANG!"));

            // Remove weapon if depleted
            if (Inventory->GetInventory()[WeaponSlot]->GetValue() <= 0)
            {
                Inventory->RemoveItem(WeaponSlot); 
            }
        }
        else
        {
            // No usable weapon -> flee
            GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("Out of ammo! Fleeing!"));

            ContextFSM->ChangeState(UFleeStateDeroyThomas::StaticClass());
        }
    }
}