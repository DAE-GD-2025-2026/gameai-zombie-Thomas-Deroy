#include "FleeState.h"
#include "ExploreState.h"
#include "../SurvivorFSM.h"
#include "Survivor/SurvivorPawn.h"

void UFleeState::Enter(USurvivorFSM* FSM)
{
    Super::Enter(FSM);
    GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("State Changed To: Flee"));

    if (ContextFSM && ContextFSM->SurvivorPawn)
    {
        ContextFSM->SurvivorPawn->StartRunning(); 
        ContextFSM->CurrentPath.Empty(); 
    }
}

void UFleeState::Update(float DeltaTime)
{
    Super::Update(DeltaTime);
    
    if (!ContextFSM || !ContextFSM->SurvivorPawn) return;

    // Determine current threat
    AActor* ThreatToEvade = nullptr;

    // Purge zone has priority over zombies
    if (IsValid(ContextFSM->ActivePurgeZone))
    {
        ThreatToEvade = ContextFSM->ActivePurgeZone;
    }
    else if (IsValid(ContextFSM->CurrentThreat))
    {
        ThreatToEvade = ContextFSM->CurrentThreat;
    }

    // Return to explore
    if (!ThreatToEvade)
    {
        ContextFSM->ChangeState(UExploreState::StaticClass());

        return;
    }

    // Check distance from threat
    float DistToThreat = FVector::Distance(ContextFSM->SurvivorPawn->GetActorLocation(), ThreatToEvade->GetActorLocation());
    
    // Larger safe distance for purge zones
    float SafeDistance = (ThreatToEvade == ContextFSM->ActivePurgeZone) ? 1400.0f : 1500.0f;

    // Stop fleeing when safe
    if (DistToThreat > SafeDistance) 
    {
        if (ThreatToEvade == ContextFSM->ActivePurgeZone)
        {
            ContextFSM->ActivePurgeZone = nullptr;
        }
        else
        {
            ContextFSM->CurrentThreat = nullptr;
        }
        
        ContextFSM->ChangeState(UExploreState::StaticClass());

        return;
    }

    // Generate escape path
    if (ContextFSM->CurrentPath.IsEmpty() || ContextFSM->CurrentPathIndex >= ContextFSM->CurrentPath.Num())
    {
        // Move away from threat
        FVector RunDirection = (ContextFSM->SurvivorPawn->GetActorLocation() - ThreatToEvade->GetActorLocation()).GetSafeNormal();

        RunDirection.Z = 0.0f;
        
        // Add randomization to movement
        RunDirection = RunDirection.RotateAngleAxis(FMath::RandRange(-30.0f, 30.0f), FVector::UpVector);

        FVector EscapeLocation = ContextFSM->SurvivorPawn->GetActorLocation() + (RunDirection * 1500.0f);

        ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(EscapeLocation);
        ContextFSM->CurrentPathIndex = 0;

        // Movement if pathfinding fails
        if (ContextFSM->CurrentPath.IsEmpty())
        {
            ContextFSM->SurvivorPawn->AddMovementInput(RunDirection, 1.0f);

            FRotator TargetRot = RunDirection.Rotation();

            FRotator SmoothRot = FMath::RInterpTo(ContextFSM->SurvivorPawn->GetActorRotation(), TargetRot, DeltaTime, 10.0f);

            ContextFSM->SurvivorPawn->SetActorRotation(SmoothRot);
        }
    }
    else
    {
        // Follow path
        ContextFSM->MoveAlongPath(DeltaTime);
    }
}

void UFleeState::Exit()
{
    Super::Exit();
    if (ContextFSM && ContextFSM->SurvivorPawn)
    {
        ContextFSM->SurvivorPawn->StopRunning(); 
        ContextFSM->CurrentPath.Empty();
    }
}