#include "CombatState.h"
#include "../SurvivorFSM.h"

void UCombatState::Enter(USurvivorFSM* FSM)
{
	Super::Enter(FSM);
	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("State Changed To: Combat"));
}

void UCombatState::Update(float DeltaTime)
{
	Super::Update(DeltaTime);
}

void UCombatState::Exit()
{
	Super::Exit();
}