#include "ExploreState.h"
#include "../SurvivorFSM.h"
#include "Survivor/SurvivorPawn.h"
#include "Village/House/House.h"

void UExploreState::Enter(USurvivorFSM* FSM)
{
	Super::Enter(FSM);
    
	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, TEXT("State Changed To: Explore"));
}

void UExploreState::Update(float DeltaTime)
{
    Super::Update(DeltaTime);
    
    if (!ContextFSM || !ContextFSM->SurvivorPawn) return;
    
    // Check if target house was reached
    if (ContextFSM->TargetHouse)
    {
        float DistToHouse = FVector::Distance(ContextFSM->SurvivorPawn->GetActorLocation(), ContextFSM->TargetHouse->GetActorLocation());

        if (DistToHouse < 300.0f) 
        {
            ContextFSM->VisitedHouses.Add(ContextFSM->TargetHouse);
            ContextFSM->KnownHouses.Remove(ContextFSM->TargetHouse);

            ContextFSM->TargetHouse = nullptr;
            ContextFSM->CurrentPath.Empty(); 
        }
    }
    
    // Generate new path if needed
    if (ContextFSM->CurrentPath.IsEmpty() || ContextFSM->CurrentPathIndex >= ContextFSM->CurrentPath.Num())
    {
        // Move to known house first
        if (ContextFSM->KnownHouses.Num() > 0 && !ContextFSM->TargetHouse)
        {
            ContextFSM->TargetHouse = ContextFSM->KnownHouses[0];
            
            ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(ContextFSM->TargetHouse->GetActorLocation());
            ContextFSM->CurrentPathIndex = 0; 

            return;
        }
        
        // Wander in random direction
        FVector ForwardDir = ContextFSM->SurvivorPawn->GetActorForwardVector();

        float RandomAngle = FMath::RandRange(-60.0f, 60.0f);

        FVector RandomDirection = ForwardDir.RotateAngleAxis(RandomAngle, FVector::UpVector);
        RandomDirection.Z = 0.0f; 
        RandomDirection.Normalize();

        FVector TargetLocation = ContextFSM->SurvivorPawn->GetActorLocation() + (RandomDirection * 1500.0f);

        ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(TargetLocation);
        ContextFSM->CurrentPathIndex = 0; 
    }
    else
    {
        // Follow current path
        ContextFSM->MoveAlongPath(DeltaTime);
    }
}

void UExploreState::Exit()
{
	Super::Exit();

	if (ContextFSM)
	{
		ContextFSM->CurrentPath.Empty(); 
	}
}