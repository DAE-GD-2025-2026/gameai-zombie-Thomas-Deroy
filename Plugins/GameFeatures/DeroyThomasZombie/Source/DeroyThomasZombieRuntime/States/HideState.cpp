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

    if (!ContextFSM || !ContextFSM->SurvivorPawn) return;
    
    ContextFSM->SurvivorPawn->StopRunning();

    // Find nearest house to hide in
    PathToClosestAvailableHouse();
}

void UHideState::Update(float DeltaTime)
{
    Super::Update(DeltaTime);

    if (!ContextFSM || !ContextFSM->SurvivorPawn) return;

    // Check if threat is still close
    EvaluateThreatDistance();

    // Move toward hiding spot
    if (ContextFSM->CurrentPathIndex < ContextFSM->CurrentPath.Num())
    {
        ContextFSM->MoveAlongPath(DeltaTime);
    }
}

void UHideState::PathToClosestAvailableHouse()
{
    AHouse* ClosestHouse = nullptr;
    float ClosestDist = FLT_MAX;

    TArray<AHouse*> AllHouses = ContextFSM->KnownHouses;
    AllHouses.Append(ContextFSM->VisitedHouses);

    for (AHouse* House : AllHouses)
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
    }
    else
    {
        // No shelter available, return to previous behavior
        ContextFSM->ResumePreviousState();
    }
}

void UHideState::EvaluateThreatDistance()
{
    if (!IsValid(ContextFSM->CurrentThreat))
    {
        ContextFSM->CurrentThreat = nullptr;
        ContextFSM->ResumePreviousState();
        return;
    }

    float ThreatDist = FVector::Distance(
        ContextFSM->SurvivorPawn->GetActorLocation(),
        ContextFSM->CurrentThreat->GetActorLocation()
    );

    // Too close -> panic decision
    if (ThreatDist < 800.0f)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("You're cooked!"));
        
        int WeaponSlot = -1;

        UHealthComponent* HealthComp = ContextFSM->SurvivorPawn->GetComponentByClass<UHealthComponent>();

        float HealthPct = HealthComp
            ? (float)HealthComp->GetHealth() / (float)HealthComp->GetMaxHealth()
            : 1.0f;

        if (ContextFSM->HasUsableWeapon(WeaponSlot) && HealthPct > 0.4f)
        {
            ContextFSM->ChangeState(UCombatState::StaticClass());
        }
        else
        {
            ContextFSM->TargetHouse = nullptr; 
            ContextFSM->ChangeState(UFleeState::StaticClass());
        }
    }
    // Safe again -> return to previous behavior
    else if (ThreatDist > 1800.0f)
    {
        ContextFSM->CurrentThreat = nullptr;
        ContextFSM->ResumePreviousState();
    }
}