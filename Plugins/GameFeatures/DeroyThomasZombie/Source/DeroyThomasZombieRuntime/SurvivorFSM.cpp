#include "SurvivorFSM.h"
#include "States/ExploreState.h"
#include "Survivor/SurvivorPawn.h" 
#include "AIController.h"
#include "DrawDebugHelpers.h"
#include "States/ExploreState.h"
#include "States/FleeState.h"
#include "States/CombatState.h"
#include "States/HideState.h"
#include "Common/InventoryComponent.h"
#include "Common/HealthComponent.h"

USurvivorFSM::USurvivorFSM()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void USurvivorFSM::BeginPlay()
{
    Super::BeginPlay();

    if (AAIController* AIController = Cast<AAIController>(GetOwner()))
    {
       SurvivorPawn = Cast<ASurvivorPawn>(AIController->GetPawn());
    }
    
    ChangeState(UExploreState::StaticClass());
}

void USurvivorFSM::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!SurvivorPawn) return;
    
    if (CurrentState)
    {
        CurrentState->Update(DeltaTime);
    }
    
    for (int i = CurrentPathIndex; i < CurrentPath.Num() - 1; ++i)
    {
       DrawDebugLine(GetWorld(), CurrentPath[i], CurrentPath[i+1], FColor::Green, false, -1.0f, 0, 2.0f);
    }
}

void USurvivorFSM::ChangeState(TSubclassOf<UBaseState> NewStateClass)
{
    if (CurrentState)
    {
        if (CurrentState->GetClass() == NewStateClass) return; 
        
        CurrentState->Exit();
    }

    if (NewStateClass)
    {
        CurrentState = NewObject<UBaseState>(this, NewStateClass);
        if (CurrentState)
        {
            CurrentState->Enter(this);
        }
    }
}

void USurvivorFSM::MoveAlongPath(float DeltaTime)
{
    if (CurrentPathIndex >= CurrentPath.Num()) return;

    FVector TargetPoint = CurrentPath[CurrentPathIndex];
    FVector MoveDirection = (TargetPoint - SurvivorPawn->GetActorLocation()).GetSafeNormal();
    MoveDirection.Z = 0.0f; 

    SurvivorPawn->AddMovementInput(MoveDirection, 1.0f);
    
    if (!MoveDirection.IsNearlyZero())
    {
       FRotator TargetRot = MoveDirection.Rotation();
       FRotator NewRot = FMath::RInterpTo(SurvivorPawn->GetActorRotation(), TargetRot, DeltaTime, 10.0f);
       SurvivorPawn->SetActorRotation(NewRot);
    }
    
    if (FVector::Distance(SurvivorPawn->GetActorLocation(), TargetPoint) < 100.0f)
    {
       CurrentPathIndex++; 
    }
}

bool USurvivorFSM::HasUsableWeapon(int& OutSlotIndex)
{
    if (UInventoryComponent* Inventory = SurvivorPawn->GetComponentByClass<UInventoryComponent>())
    {
        auto const& Items = Inventory->GetInventory();
        for (int i = 0; i < Items.Num(); ++i)
        {
            if (Items[i] != nullptr)
            {
                EItemType Type = Items[i]->GetItemType();
                // If weapon and has ammo
                if ((Type == EItemType::Pistol || Type == EItemType::Shotgun) && Items[i]->GetValue() > 0)
                {
                    OutSlotIndex = i;
                    return true;
                }
            }
        }
    }
    return false;
}

void USurvivorFSM::OnZombieSpotted(AActor* Zombie)
{
    CurrentThreat = Zombie;

    int WeaponSlot = -1;
    UHealthComponent* HealthComp = SurvivorPawn->GetComponentByClass<UHealthComponent>();
    float HealthPct = HealthComp ? (float)HealthComp->GetHealth() / (float)HealthComp->GetMaxHealth() : 1.0f;

    // Check if there is valid house in memory
    bool bHasValidHouse = false;
    for (AHouse* House : KnownHouses) {
        if (IsValid(House)) { bHasValidHouse = true; break; }
    }
    if (!bHasValidHouse) {
        for (AHouse* House : VisitedHouses) {
            if (IsValid(House)) { bHasValidHouse = true; break; }
        }
    }
    
    // Use weapon
    if (HasUsableWeapon(WeaponSlot) && HealthPct > 0.4f)
    {
        ChangeState(UCombatState::StaticClass());
    }
    // Hide in house
    else if (bHasValidHouse)
    {
        ChangeState(UHideState::StaticClass());
    }
    // Run
    else
    {
        ChangeState(UFleeState::StaticClass()); 
    }
}