#include "HideState.h"
#include "../SurvivorFSM.h"
#include "ExploreState.h"
#include "Survivor/SurvivorPawn.h"
#include "Village/House/House.h"
#include "FleeState.h" 
#include "CombatState.h"
#include "Common/HealthComponent.h"

void UHideState::Enter(USurvivorFSM* FSM)
{
    Super::Enter(FSM);
    GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, TEXT("State Changed To: Hide"));

    if (!ContextFSM) return;
    
    ContextFSM->SurvivorPawn->StopRunning();

    // Find closest house known
    AHouse* ClosestHouse = nullptr;
    float ClosestDist = FLT_MAX;

    // Check all known and visited houses
    TArray<AHouse*> AllHouses = ContextFSM->KnownHouses;
    AllHouses.Append(ContextFSM->VisitedHouses);

    for (AHouse* House : AllHouses)
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
        // Run to house
        ContextFSM->TargetHouse = ClosestHouse;
        ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(ClosestHouse->GetBounds().Origin);
        ContextFSM->CurrentPathIndex = 0;
    }
    else
    {
        // No house
        ContextFSM->ChangeState(UExploreState::StaticClass());
    }
}

void UHideState::Update(float DeltaTime)
{
    Super::Update(DeltaTime);
    
    if (!ContextFSM || !ContextFSM->SurvivorPawn) return;

    // Check for threat
    if (IsValid(ContextFSM->CurrentThreat))
    {
        float ThreatDist = FVector::Distance(ContextFSM->SurvivorPawn->GetActorLocation(), ContextFSM->CurrentThreat->GetActorLocation());

        if (ThreatDist < 500.0f) // Zombie is inside house
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("You're cooked!"));
            
            int WeaponSlot = -1;
            UHealthComponent* HealthComp = ContextFSM->SurvivorPawn->GetComponentByClass<UHealthComponent>();
            float HealthPct = HealthComp ? (float)HealthComp->GetHealth() / (float)HealthComp->GetMaxHealth() : 1.0f;

            // We have weapon
            if (ContextFSM->HasUsableWeapon(WeaponSlot) && HealthPct > 0.4f)
            {
                ContextFSM->ChangeState(UCombatState::StaticClass());
            }
            else // We don't have weapon
            {
                if (ContextFSM->TargetHouse)
                {
                    ContextFSM->VisitedHouses.Add(ContextFSM->TargetHouse);
                    ContextFSM->KnownHouses.Remove(ContextFSM->TargetHouse);
                }
                ContextFSM->ChangeState(UFleeState::StaticClass());
            }
            return;
        }
        
        // Return explore state when safe
        if (ThreatDist > 1800.0f)
        {
            ContextFSM->CurrentThreat = nullptr;
            ContextFSM->ChangeState(UExploreState::StaticClass());

            return;
        }
    }
    else
    {
        // Return explore state when safe
        ContextFSM->CurrentThreat = nullptr;
        ContextFSM->ChangeState(UExploreState::StaticClass());

        return;
    }

    // Move to hiding spot
    if (ContextFSM->CurrentPathIndex < ContextFSM->CurrentPath.Num())
    {
        ContextFSM->MoveAlongPath(DeltaTime);
    }
}