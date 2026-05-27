#include "ExploreState.h"
#include "../SurvivorFSM.h"
#include "Survivor/SurvivorPawn.h"

void UExploreState::Enter(USurvivorFSM* FSM)
{
	Super::Enter(FSM);
	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, TEXT("State Changed To: Explore"));
}

void UExploreState::Update(float DeltaTime)
{
	Super::Update(DeltaTime);

	if (!ContextFSM || !ContextFSM->SurvivorPawn) return;

	if (ContextFSM->CurrentPath.IsEmpty() || ContextFSM->CurrentPathIndex >= ContextFSM->CurrentPath.Num())
	{
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