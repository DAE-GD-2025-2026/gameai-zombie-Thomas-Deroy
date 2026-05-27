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
    
    if (!ContextFSM->CurrentThreat)
    {
        ContextFSM->ChangeState(UExploreState::StaticClass());
        return;
    }
    
    float DistToThreat = FVector::Distance(ContextFSM->SurvivorPawn->GetActorLocation(), ContextFSM->CurrentThreat->GetActorLocation());
    if (DistToThreat > 1500.0f) 
    {
        ContextFSM->CurrentThreat = nullptr;
        ContextFSM->ChangeState(UExploreState::StaticClass());
        return;
    }
    
    if (ContextFSM->CurrentPath.IsEmpty() || ContextFSM->CurrentPathIndex >= ContextFSM->CurrentPath.Num())
    {
        FVector RunDirection = (ContextFSM->SurvivorPawn->GetActorLocation() - ContextFSM->CurrentThreat->GetActorLocation()).GetSafeNormal();
        RunDirection.Z = 0.0f;
        
        RunDirection = RunDirection.RotateAngleAxis(FMath::RandRange(-30.0f, 30.0f), FVector::UpVector);

        FVector EscapeLocation = ContextFSM->SurvivorPawn->GetActorLocation() + (RunDirection * 1000.0f);
        ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(EscapeLocation);
        ContextFSM->CurrentPathIndex = 0;
    }
    else
    {
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