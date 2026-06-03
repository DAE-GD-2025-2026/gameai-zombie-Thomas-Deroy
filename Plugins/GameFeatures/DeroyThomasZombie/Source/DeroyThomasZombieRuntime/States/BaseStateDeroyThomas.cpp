#include "BaseStateDeroyThomas.h"
#include "../SurvivorFSMDeroyThomas.h"

void UBaseStateDeroyThomas::Enter(USurvivorFSMDeroyThomas* FSM)
{
	ContextFSM = FSM;
}

void UBaseStateDeroyThomas::Update(float DeltaTime)
{

}

void UBaseStateDeroyThomas::Exit()
{
	ContextFSM = nullptr;
}