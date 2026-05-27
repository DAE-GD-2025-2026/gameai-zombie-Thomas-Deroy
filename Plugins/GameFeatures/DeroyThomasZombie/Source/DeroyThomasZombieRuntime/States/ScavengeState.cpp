#include "ScavengeState.h"
#include "../SurvivorFSM.h"

void UScavengeState::Enter(USurvivorFSM* FSM)
{
	Super::Enter(FSM);
	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple, TEXT("State Changed To: Scavenge"));
}

void UScavengeState::Update(float DeltaTime)
{
	Super::Update(DeltaTime);
}

void UScavengeState::Exit()
{
	Super::Exit();
}