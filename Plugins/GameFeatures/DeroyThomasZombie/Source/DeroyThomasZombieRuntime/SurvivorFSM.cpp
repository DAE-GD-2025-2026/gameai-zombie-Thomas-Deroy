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
    
    UHealthComponent* HealthComp = SurvivorPawn->GetComponentByClass<UHealthComponent>();
    if (HealthComp)
    {
        int CurrentHealth = HealthComp->GetHealth();

        // Get Health
        if (LastKnownHealth == -1) 
        {
            LastKnownHealth = CurrentHealth;
        }

        // Track health
        if (CurrentHealth < LastKnownHealth)
        {
            // Panic
            if (!CurrentThreat && !ActivePurgeZone)
            {
                bIsReflexSpinning = true;
                
                ReflexTargetLocation = SurvivorPawn->GetActorLocation() - (SurvivorPawn->GetActorForwardVector() * 500.0f);
                GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Something bit me! Whipping around!"));
            }
        }
        
        // Remember new health
        LastKnownHealth = CurrentHealth;
    }
    
    EvaluateInventory();
    
    if (bIsReflexSpinning && SurvivorPawn)
    {
        FVector DirToDamage = (ReflexTargetLocation - SurvivorPawn->GetActorLocation()).GetSafeNormal();
        DirToDamage.Z = 0.0f;
        
        // Spin
        FRotator TargetRot = DirToDamage.Rotation();
        FRotator SmoothRot = FMath::RInterpTo(SurvivorPawn->GetActorRotation(), TargetRot, DeltaTime, 40.0f);
        SurvivorPawn->SetActorRotation(SmoothRot);
        
        if (FVector::DotProduct(SurvivorPawn->GetActorForwardVector(), DirToDamage) > 0.95f)
        {
            bIsReflexSpinning = false;
        }
        
        return;
    }
    
    if (CurrentState)
    {
        CurrentState->Update(DeltaTime);
    }
    
    // Visualize debug
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC && PC->WasInputKeyJustPressed(EKeys::One))
    {
        bShowDebug = !bShowDebug; // Toggle
    }

    if (bShowDebug && SurvivorPawn)
    {
        // Draw path
        for (int i = CurrentPathIndex; i < CurrentPath.Num() - 1; ++i)
        {
            DrawDebugLine(GetWorld(), CurrentPath[i], CurrentPath[i+1], FColor::Green, false, -1.0f, 0, 2.0f);
        }

        // Draw vision cone
        FVector PawnLoc = SurvivorPawn->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
        FVector Forward = SurvivorPawn->GetActorForwardVector();
        float VisionAngleWidth = FMath::DegreesToRadians(70.0f); 
        float VisionAngleHeight = FMath::DegreesToRadians(5.0f); 
        DrawDebugCone(GetWorld(), PawnLoc, Forward, 1000.0f, VisionAngleWidth, VisionAngleHeight, 24, FColor::Yellow, false, -1.0f, 0, 1.5f);

        // Draw memory radar
        for (AActor* Zombie : KnownZombies) 
        {
            if (IsValid(Zombie)) DrawDebugLine(GetWorld(), PawnLoc, Zombie->GetActorLocation(), FColor::Red, false, -1.0f, 0, 1.0f);
        }
        for (AHouse* House : KnownHouses) 
        {
            if (IsValid(House)) DrawDebugLine(GetWorld(), PawnLoc, House->GetActorLocation(), FColor::Blue, false, -1.0f, 0, 1.0f);
        }

        // Information
        FVector HeadLoc = SurvivorPawn->GetActorLocation() + FVector(0.0f, 0.0f, 150.0f);
        
        UHealthComponent* HC = SurvivorPawn->GetComponentByClass<UHealthComponent>();
        UStaminaComponent* SC = SurvivorPawn->GetComponentByClass<UStaminaComponent>();
        
        FString DebugText;
        if (CurrentState)
        {
            FString StateName = CurrentState->GetClass()->GetName();
            StateName.RemoveFromStart("U"); 
            DebugText += FString::Printf(TEXT("\nSTATE: %s"), *StateName);
        }

        if (CurrentThreat) DebugText += TEXT("\nTargeting: ZOMBIE RUN");
        if (ActivePurgeZone) DebugText += TEXT("\nTargeting: PURGE ZONE RUN");
        
        DrawDebugString(GetWorld(), HeadLoc, DebugText, nullptr, FColor::Cyan, 0.0f, true, 1.2f);
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
        float DynamicRotSpeed = FMath::GetMappedRangeValueClamped(FVector2D(1.0f, -1.0f), FVector2D(15.0f, 45.0f), DirectionDot);
       
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

bool USurvivorFSM::GetBestWeapon(float TargetDistance, int& OutSlotIndex)
{
    UInventoryComponent* Inventory = SurvivorPawn->GetComponentByClass<UInventoryComponent>();
    if (!Inventory) return false;

    int BestShotgun = -1;
    int BestPistol = -1;

    auto const& Items = Inventory->GetInventory();
    for (int i = 0; i < Items.Num(); ++i)
    {
        if (Items[i] != nullptr && Items[i]->GetValue() > 0)
        {
            if (Items[i]->GetItemType() == EItemType::Shotgun) BestShotgun = i;
            if (Items[i]->GetItemType() == EItemType::Pistol) BestPistol = i;
        }
    }

    // Shotgun close range, Pistol long range
    if (TargetDistance < 250.0f)
    {
        if (BestShotgun != -1) { OutSlotIndex = BestShotgun; return true; }
        if (BestPistol != -1) { OutSlotIndex = BestPistol; return true; }
    }
    else
    {
        if (BestPistol != -1) { OutSlotIndex = BestPistol; return true; }
        if (BestShotgun != -1) { OutSlotIndex = BestShotgun; return true; }
    }

    return false;
}

void USurvivorFSM::OnZombieSpotted(AActor* Zombie)
{
    // Zombie is far
    float DistToZombie = FVector::Distance(SurvivorPawn->GetActorLocation(), Zombie->GetActorLocation());
    if (DistToZombie > 700.0f)
    {
        return; 
    }
    
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
    
    // Keep distance from heavy
    bool bIsHeavy = Zombie->GetClass()->GetName().Contains("Heavy");
    
    if (bIsHeavy && DistToZombie > 300.0f)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Orange, TEXT("Just a Heavy. Saving ammo and walking away!"));
        ChangeState(UFleeState::StaticClass());
        return;
    }
    
    // Use weapon
    if (GetBestWeapon(DistToZombie, WeaponSlot) && HealthPct > 0.4)
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

void USurvivorFSM::EvaluateInventory()
{
    if (!SurvivorPawn) return;
    
    UInventoryComponent* Inventory = SurvivorPawn->GetComponentByClass<UInventoryComponent>();
    UHealthComponent* HealthComp = SurvivorPawn->GetComponentByClass<UHealthComponent>();
    UStaminaComponent* StaminaComp = SurvivorPawn->GetComponentByClass<UStaminaComponent>();

    if (!Inventory || !HealthComp || !StaminaComp) return;

    // Check  vitals
    float HealthPercentage = (float)HealthComp->GetHealth() / (float)HealthComp->GetMaxHealth();
    float StaminaPercentage = StaminaComp->GetCurrentStamina() / StaminaComp->GetMaxStamina();

    auto const& Items = Inventory->GetInventory();

    // Check items
    for (int i = 0; i < Items.Num(); ++i)
    {
        if (ABaseItem* Item = Items[i])
        {
            // Throw the garbage
            if (Item->GetItemType() == EItemType::Garbage)
            {
                Inventory->RemoveItem(i); 
                GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::White, TEXT("Tossed some Garbage!"));
                continue;
            }
            
            // Use medkit if health low
            if (Item->GetItemType() == EItemType::Medkit && HealthPercentage < 0.4f)
            {
                Inventory->UseItem(i);
                if (Item->GetValue() <= 0)
                {
                    Inventory->RemoveItem(i); 
                }
                GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Used Medkit!"));
                break; 
            }
            // Eat food if stamina low
            else if (Item->GetItemType() == EItemType::Food && StaminaPercentage < 0.5f)
            {
                Inventory->UseItem(i);
                if (Item->GetValue() <= 0)
                {
                    Inventory->RemoveItem(i);
                }
                GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("Ate Food!"));
                break;
            }
        }
    }
}

void USurvivorFSM::OnDamageSensed(FVector DamageLocation)
{
    if (CurrentThreat || ActivePurgeZone) return;

    // We got hit
    bIsReflexSpinning = true;
    ReflexTargetLocation = SurvivorPawn->GetActorLocation() - (SurvivorPawn->GetActorForwardVector() * 500.0f);
    GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Who hit me?!"));
}

AActor* USurvivorFSM::GetClosestKnownItem(EItemType DesiredType)
{
    AActor* BestItem = nullptr;
    float ClosestDist = 999999.0f;
    
    for (int i = KnownItems.Num() - 1; i >= 0; --i)
    {
        AActor* ItemActor = KnownItems[i];
        if (!IsValid(ItemActor)) 
        {
            KnownItems.RemoveAt(i);
            continue;
        }

        if (ABaseItem* Item = Cast<ABaseItem>(ItemActor))
        {
            if (Item->GetItemType() == DesiredType)
            {
                float Dist = FVector::Distance(SurvivorPawn->GetActorLocation(), Item->GetActorLocation());
                if (Dist < ClosestDist)
                {
                    ClosestDist = Dist;
                    BestItem = ItemActor;
                }
            }
        }
    }
    return BestItem;
}