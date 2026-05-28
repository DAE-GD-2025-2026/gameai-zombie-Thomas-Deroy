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

    // Debug cone
    if (SurvivorPawn)
    {
        FVector PawnLoc = SurvivorPawn->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
        FVector Forward = SurvivorPawn->GetActorForwardVector();
    
        float SightRadius = 1000.0f;
        
        float VisionAngleWidth = FMath::DegreesToRadians(70.0f); 
        float VisionAngleHeight = FMath::DegreesToRadians(5.0f); 
    
        DrawDebugCone(
            GetWorld(), 
            PawnLoc, 
            Forward, 
            SightRadius, 
            VisionAngleWidth, 
            VisionAngleHeight, 
            24,                
            FColor::Yellow, 
            false, 
            -1.0f, 
            0, 
            1.5f               
        );
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
    if (!SurvivorPawn || CurrentPath.IsEmpty()) return;

    FVector PawnLocation = SurvivorPawn->GetActorLocation();
    
    float WaypointTolerance = 75.0f; 

    while (CurrentPathIndex < CurrentPath.Num() - 1 && FVector::Distance(PawnLocation, CurrentPath[CurrentPathIndex]) < WaypointTolerance)
    {
        CurrentPathIndex++;
    }

    // Stop if path finished
    if (CurrentPathIndex >= CurrentPath.Num()) return;

    FVector TargetPoint = CurrentPath[CurrentPathIndex];

    // Direction towards waypoint
    FVector SeekForce = (TargetPoint - PawnLocation).GetSafeNormal();
    SeekForce.Z = 0.0f;

    // Force used to avoid nearby zombies
    FVector SeparationForce = FVector::ZeroVector;
    int AvoidCount = 0;
    
    // Remove invalid zombie references
    for (int i = KnownZombies.Num() - 1; i >= 0; --i)
    {
        if (!IsValid(KnownZombies[i]))
        {
            KnownZombies.RemoveAt(i);
        }
    }

    // Calculate avoidance force
    for (AActor* Zombie : KnownZombies)
    {
        float DistToZombie = FVector::Distance(PawnLocation, Zombie->GetActorLocation());
        
        // Avoid zombies within this radius
        float AvoidanceRadius = 400.0f; 

        if (DistToZombie < AvoidanceRadius)
        {
            FVector PushAway = (PawnLocation - Zombie->GetActorLocation()).GetSafeNormal();

            // Push when closer to zombie
            PushAway *= (AvoidanceRadius - DistToZombie) / AvoidanceRadius; 

            SeparationForce += PushAway;
            AvoidCount++;
        }
    }

    // Average all avoidance forces
    if (AvoidCount > 0)
    {
        SeparationForce /= AvoidCount;
    }

    SeparationForce.Z = 0.0f;

    // Blend movement forces
    float SeekWeight = 1.0f;
    float SeparationWeight = 2.0f; 

    FVector DesiredDirection = (SeekForce * SeekWeight) + (SeparationForce * SeparationWeight);

    DesiredDirection.Z = 0.0f;
    DesiredDirection.Normalize(); 

    // Apply movement input
    SurvivorPawn->AddMovementInput(DesiredDirection, 1.0f);

    // Smoothly rotate towards movement direction
    if (!DesiredDirection.IsNearlyZero(0.1f))
    {
       float DirectionDot = FVector::DotProduct(SurvivorPawn->GetActorForwardVector(), DesiredDirection);
       
       // Rotate faster during sharp turns
       float DynamicRotSpeed = FMath::GetMappedRangeValueClamped(
           FVector2D(1.0f, -1.0f),
           FVector2D(8.0f, 25.0f),
           DirectionDot
       );
       
       FRotator TargetRot = DesiredDirection.Rotation();

       FRotator SmoothRot = FMath::RInterpTo(
           SurvivorPawn->GetActorRotation(),
           TargetRot,
           DeltaTime,
           DynamicRotSpeed
       );

       SurvivorPawn->SetActorRotation(SmoothRot);
    }

    // Smaller tolerance for final waypoint
    float ArrivalTolerance = (CurrentPathIndex == CurrentPath.Num() - 1) ? 50.0f : 100.0f;

    // Advance to next waypoint
    if (FVector::Distance(PawnLocation, TargetPoint) < ArrivalTolerance)
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

void USurvivorFSM::OnPurgeZoneSpotted(AActor* PurgeZone)
{
    ActivePurgeZone = PurgeZone;
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Purge zone, run broski!"));
    
    // Overwrite to flee
    ChangeState(UFleeState::StaticClass()); 
}

void USurvivorFSM::OnPurgeZoneLost(AActor* PurgeZone)
{
    if (ActivePurgeZone == PurgeZone)
    {
        ActivePurgeZone = nullptr;
    }
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