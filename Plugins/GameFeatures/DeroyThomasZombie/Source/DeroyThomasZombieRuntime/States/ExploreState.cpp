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
        // Distance to destination
        float DistToDestination = FLT_MAX;

        if (ContextFSM->CurrentPath.Num() > 0)
        {
            DistToDestination = FVector::Distance(ContextFSM->SurvivorPawn->GetActorLocation(), ContextFSM->CurrentPath.Last());
        }
        
        // Generate search point when we are there
        if (DistToDestination < 100.0f || ContextFSM->CurrentPath.IsEmpty() || ContextFSM->CurrentPathIndex >= ContextFSM->CurrentPath.Num())
        {
            if (SearchWaypoints > 0)
            {
                // Continue searching
                SearchWaypoints--;

                FHouseBounds Bounds = ContextFSM->TargetHouse->GetBounds();
                
                // Random point inside house bounds
                FVector RandomOffset;

                RandomOffset.X = FMath::RandRange(-Bounds.Extent.X * 0.7f, Bounds.Extent.X * 0.7f);
                RandomOffset.Y = FMath::RandRange(-Bounds.Extent.Y * 0.7f, Bounds.Extent.Y * 0.7f);
                RandomOffset.Z = 0.0f;

                FVector SearchPoint = Bounds.Origin + RandomOffset;

                ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(SearchPoint);
                ContextFSM->CurrentPathIndex = 0;

                return;
            }
            else
            {
                // Fully searched
                ContextFSM->VisitedHouses.Add(ContextFSM->TargetHouse);
                ContextFSM->KnownHouses.Remove(ContextFSM->TargetHouse);

                ContextFSM->TargetHouse = nullptr;
                ContextFSM->CurrentPath.Empty(); 
            }
        }
    }
    
    // Generate new path if needed
    if (ContextFSM->CurrentPath.IsEmpty() || ContextFSM->CurrentPathIndex >= ContextFSM->CurrentPath.Num())
    {
        // Move to known house first
        if (ContextFSM->KnownHouses.Num() > 0 && !ContextFSM->TargetHouse)
        {
            ContextFSM->TargetHouse = ContextFSM->KnownHouses[0];
            SearchWaypoints = 2; 
            
            ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(ContextFSM->TargetHouse->GetBounds().Origin);
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