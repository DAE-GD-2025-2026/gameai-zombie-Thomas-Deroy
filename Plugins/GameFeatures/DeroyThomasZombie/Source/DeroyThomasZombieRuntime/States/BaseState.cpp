#include "BaseState.h"
#include "../SurvivorFSM.h"

void UBaseState::Enter(USurvivorFSM* FSM)
{
	ContextFSM = FSM;
}

void UBaseState::Update(float DeltaTime)
{

}

void UBaseState::Exit()
{
	ContextFSM = nullptr;
}