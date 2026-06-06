#include "SurvivorFSMDeroyThomas.h"
#include "States/ExploreStateDeroyThomas.h"
#include "Survivor/SurvivorPawn.h" 
#include "AIController.h"
#include "DrawDebugHelpers.h"
#include "States/ExploreStateDeroyThomas.h"
#include "States/FleeStateDeroyThomas.h"
#include "States/CombatStateDeroyThomas.h"
#include "States/HideStateDeroyThomas.h"
#include "Common/InventoryComponent.h"
#include "Common/HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "States/ReturnStateDeroyThomas.h"
#include "States/ScavengeStateDeroyThomas.h"

USurvivorFSMDeroyThomas::USurvivorFSMDeroyThomas()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void USurvivorFSMDeroyThomas::BeginPlay()
{
    Super::BeginPlay();

    bShowDebug = true; 

    if (AAIController* AIController = Cast<AAIController>(GetOwner()))
    {
        SurvivorPawn = Cast<ASurvivorPawn>(AIController->GetPawn());
    }
    
    TArray<AActor*> FoundHouses;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AHouse::StaticClass(), FoundHouses);
    for (AActor* ActorHouse : FoundHouses)
    {
        if (AHouse* House = Cast<AHouse>(ActorHouse))
        {
            AllLevelHouses.Add(House);
        }
    }
    
    TArray<AActor*> FoundItems;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABaseItem::StaticClass(), FoundItems);
    for (AActor* ActorItem : FoundItems)
    {
        if (ABaseItem* Item = Cast<ABaseItem>(ActorItem))
        {
            AllLevelItems.Add(Item);
        }
    }
    
    ChangeState(UExploreStateDeroyThomas::StaticClass());
}

void USurvivorFSMDeroyThomas::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!SurvivorPawn) return;
    
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC)
    {
        if (PC->WasInputKeyJustPressed(EKeys::One)) bShowDebug = !bShowDebug; 
        if (PC->WasInputKeyJustPressed(EKeys::Two)) bShowEnemyDebug = !bShowEnemyDebug; 
        if (PC->WasInputKeyJustPressed(EKeys::Three)) bShowMovementDebug = !bShowMovementDebug; 
    }

    if (bShowDebug) HandleDebugDrawing();
    
    UpdateHealthMonitor();
    EvaluateInventory();
    
    if (HandleReflexSpinning(DeltaTime)) return;
    
    MonitorComfortZone();
    
    if (CurrentState) CurrentState->Update(DeltaTime);
}

void USurvivorFSMDeroyThomas::UpdateHealthMonitor()
{
    UHealthComponent* HealthComp = SurvivorPawn->GetComponentByClass<UHealthComponent>();
    if (HealthComp)
    {
        int CurrentHealth = HealthComp->GetHealth();

        // First update initialize value
        if (LastKnownHealth == -1) LastKnownHealth = CurrentHealth;

        // Took damage since last time
        if (CurrentHealth < LastKnownHealth) OnDamageSensed(FVector::ZeroVector);

        LastKnownHealth = CurrentHealth;
    }
}

bool USurvivorFSMDeroyThomas::HandleReflexSpinning(float DeltaTime)
{
    if (!bIsReflexSpinning || !SurvivorPawn) return false;

    // Turn to damage 
    FVector DirToDamage = (ReflexTargetLocation - SurvivorPawn->GetActorLocation()).GetSafeNormal();
    DirToDamage.Z = 0.0f;
    
    FRotator TargetRot = DirToDamage.Rotation();
    FRotator SmoothRot = FMath::RInterpTo(SurvivorPawn->GetActorRotation(), TargetRot, DeltaTime, 40.0f);
    SurvivorPawn->SetActorRotation(SmoothRot);
    
    // Finished turning
    if (FVector::DotProduct(SurvivorPawn->GetActorForwardVector(), DirToDamage) > 0.95f)
    {
        bIsReflexSpinning = false;
    }

    return true;
}

void USurvivorFSMDeroyThomas::MonitorComfortZone()
{
    if (IsValid(ActivePurgeZone))
    {
        float DistToPurge = FVector::Distance(SurvivorPawn->GetActorLocation(), ActivePurgeZone->GetActorLocation());
        
        // Panic if in it
        if (DistToPurge < 800.0f)
        {
            if (CurrentState && CurrentState->GetClass() != UFleeStateDeroyThomas::StaticClass())
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Purge Zone Threat! Evacuating!"));
                ChangeState(UFleeStateDeroyThomas::StaticClass());
                return;
            }
        }
    }
    
    if (CurrentState && (CurrentState->GetClass() == UExploreStateDeroyThomas::StaticClass() || 
                         CurrentState->GetClass() == UReturnStateDeroyThomas::StaticClass() || 
                         CurrentState->GetClass() == UScavengeStateDeroyThomas::StaticClass()))
    {
        for (AActor* Zombie : KnownZombies)
        {
            if (!IsValid(Zombie)) continue;
            
            float Dist = FVector::Distance(SurvivorPawn->GetActorLocation(), Zombie->GetActorLocation());
            bool bIsHeavy = Zombie->GetClass()->GetName().Contains("Heavy");

            // Heavy zombies smaller comfort zone
            if ((bIsHeavy && Dist < 250.0f) || (!bIsHeavy && Dist < 450.0f))
            {
                OnZombieSpotted(Zombie);
                break;
            }
        }
    }
}

void USurvivorFSMDeroyThomas::HandleDebugDrawing()
{
    FVector PawnLoc = SurvivorPawn->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
    
    if (bShowMovementDebug)
    {
        // Current path
        for (int i = CurrentPathIndex; i < CurrentPath.Num() - 1; ++i)
            DrawDebugLine(GetWorld(), CurrentPath[i], CurrentPath[i+1], FColor::Green, false, -1.0f, 0, 2.0f);

        // Vision check area
        FVector Forward = SurvivorPawn->GetActorForwardVector();
        DrawDebugCone(GetWorld(), PawnLoc, Forward, 500.0f, FMath::DegreesToRadians(70.0f), FMath::DegreesToRadians(5.0f), 24, FColor::Yellow, false, -1.0f, 0, 1.5f);
        DrawDebugString(GetWorld(), PawnLoc + (Forward * 500.0f), TEXT("[PARANOIA CHECK CONE]"), nullptr, FColor::Yellow, 0.0f, true, 1.0f);

        // Current movement direction
        if (!CurrentVelocity.IsNearlyZero())
        {
            FVector VelocityEnd = PawnLoc + (CurrentVelocity.GetSafeNormal() * 150.0f);
            DrawDebugDirectionalArrow(GetWorld(), PawnLoc, VelocityEnd, 40.0f, FColor::Magenta, false, -1.0f, 0, 5.0f);
            DrawDebugString(GetWorld(), VelocityEnd + FVector(0,0,20.0f), TEXT("[BLENDED STEERING]"), nullptr, FColor::Magenta, 0.0f, true, 1.0f);
        }
        
        // Known and visited houses
        for (AHouse* House : AllLevelHouses)
        {
            if (!IsValid(House)) continue;

            FColor HouseColor = FColor::White;
            bool bIsTracked = false;
            FString HouseTag = TEXT("");

            if (VisitedHouses.Contains(House))
            {
                HouseColor = FColor::Black;
                bIsTracked = true;
                HouseTag = TEXT("MEMORY MAP: VISITED");
            }
            else if (KnownHouses.Contains(House))
            {
                HouseColor = FColor::Blue;
                bIsTracked = true;
                HouseTag = TEXT("MEMORY MAP: KNOWN");
            }
            
            FHouseBounds Bounds = House->GetBounds();
            DrawDebugBox(GetWorld(), Bounds.Origin, Bounds.Extent, FQuat::Identity, HouseColor, false, -1.0f, 0, 8.0f);
            
            if (bIsTracked)
            {
                FVector TextLoc = House->GetActorLocation() + FVector(0.0f, 0.0f, 400.0f);
                DrawDebugString(GetWorld(), TextLoc, HouseTag, nullptr, HouseColor, 0.0f, true, 1.5f);
                DrawDebugLine(GetWorld(), PawnLoc, House->GetActorLocation(), HouseColor, false, -1.0f, 0, 1.5f);
            }
        }

        // Remembered items
        TArray<AActor*> DynamicFoundItems;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABaseItem::StaticClass(), DynamicFoundItems);
        UInventoryComponent* Inv = SurvivorPawn->GetComponentByClass<UInventoryComponent>();

        for (AActor* ActorItem : DynamicFoundItems)
        {
            ABaseItem* Item = Cast<ABaseItem>(ActorItem);

            if (!Item || Item->GetOwner() != nullptr || (Inv && Inv->GetInventory().Contains(Item)) || Item->GetItemType() == EItemType::Garbage)
                continue;

            FColor ItemColor = KnownItems.Contains(Item) ? FColor::Cyan : FColor::White;
            if (KnownItems.Contains(Item))
            {
                DrawDebugLine(GetWorld(), PawnLoc, Item->GetActorLocation(), FColor::Cyan, false, -1.0f, 0, 1.0f);
                DrawDebugString(GetWorld(), Item->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f), TEXT("[RESTOCK MEMORY]"), nullptr, FColor::Cyan, 0.0f, true, 1.0f);
            }
            DrawDebugBox(GetWorld(), Item->GetActorLocation(), FVector(15.0f, 15.0f, 15.0f), FQuat::Identity, ItemColor, false, -1.0f, 0, 3.0f);
        }
    }
    
    if (bShowEnemyDebug)
    {
        // Track nearby threats
        for (AActor* Zombie : KnownZombies) 
        {
            if (!IsValid(Zombie)) continue;

            FColor ZombieColor = FColor::Yellow;
            float DistToZombie = FVector::Distance(PawnLoc, Zombie->GetActorLocation());
            
            if (Zombie == CurrentThreat)
            {
                ZombieColor = FColor::Red;

                // Current target
                DrawDebugLine(GetWorld(), PawnLoc, Zombie->GetActorLocation() + FVector(0,0,30), FColor::Red, false, -1.0f, 0, 4.0f);
                DrawDebugBox(GetWorld(), Zombie->GetActorLocation(), FVector(55.0f, 55.0f, 100.0f), FQuat::Identity, FColor::Red, false, -1.0f, 0, 4.0f);
                DrawDebugString(GetWorld(), Zombie->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f), FString::Printf(TEXT("[TARGET LOCKED]\nDistance: %.0f"), DistToZombie), nullptr, FColor::Red, 0.0f, true, 1.5f);
            }
            else
            {
                DrawDebugString(GetWorld(), Zombie->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f), FString::Printf(TEXT("Dist: %.0f"), DistToZombie), nullptr, ZombieColor, 0.0f, true, 1.0f);
            }

            DrawDebugCapsule(GetWorld(), Zombie->GetActorLocation(), 90.0f, 40.0f, FQuat::Identity, ZombieColor, false, -1.0f, 0, 5.0f);
        }

        // Current state and priorities
        FString DebugText;
        if (CurrentState)
        {
            FString StateName = CurrentState->GetClass()->GetName();
            StateName.RemoveFromStart("U");
            StateName.RemoveFromEnd("DeroyThomas");
            DebugText += FString::Printf(TEXT("STATE: %s\nRESTOCK TIMER: %.1f"), *StateName, TimeSinceLastTown);
        }

        if (CurrentThreat) DebugText += TEXT("\nTargeting: ZOMBIE RUN");
        if (ActivePurgeZone) DebugText += TEXT("\nTargeting: PURGE ZONE RUN");

        DrawDebugString(GetWorld(), SurvivorPawn->GetActorLocation() + FVector(0.0f, 0.0f, 220.0f), DebugText, nullptr, FColor::Cyan, 0.0f, true, 1.2f);
    }
}

void USurvivorFSMDeroyThomas::ChangeState(TSubclassOf<UBaseStateDeroyThomas> NewStateClass)
{
    if (CurrentState)
    {
        if (CurrentState->GetClass() == NewStateClass) return; 
        
        UClass* CurrentClass = CurrentState->GetClass();
        if (CurrentClass != UFleeStateDeroyThomas::StaticClass() && 
            CurrentClass != UCombatStateDeroyThomas::StaticClass() && 
            CurrentClass != UHideStateDeroyThomas::StaticClass())
        {
            PreviousStateClass = CurrentClass;
        }
        
        CurrentState->Exit();
    }

    if (NewStateClass)
    {
        CurrentState = NewObject<UBaseStateDeroyThomas>(this, NewStateClass);
        if (CurrentState)
        {
            CurrentState->Enter(this);
        }
    }
}

void USurvivorFSMDeroyThomas::ResumePreviousState()
{
    if (PreviousStateClass)
    {
        ChangeState(PreviousStateClass);
    }
    else
    {
        ChangeState(UExploreStateDeroyThomas::StaticClass());
    }
}

void USurvivorFSMDeroyThomas::MoveAlongPath(float DeltaTime)
{
    if (!SurvivorPawn || CurrentPath.IsEmpty()) return;

    FVector PawnLocation = SurvivorPawn->GetActorLocation();
    
    // Detect when the agent becomes stuck
    float Speed = FVector::Distance(PawnLocation, LastFrameLocation) / DeltaTime;
    LastFrameLocation = PawnLocation;

    // If movement is extremely slow, start stuck recovery
    if (Speed < 15.0f)
    {
        StuckTimer += DeltaTime;
    
        // Fully abandon the path if stuck too long
        if (StuckTimer > 1.2f)
        {
            CurrentPath.Empty();
            StuckTimer = 0.0f;

            GEngine->AddOnScreenDebugMessage(
                -1,
                3.f,
                FColor::Red,
                TEXT("Corner locked! Nuking path to recalculate.")
            );

            return;
        }
        // Skip a waypoint if lightly snagged
        else if (StuckTimer > 0.4f)
        {
            if (CurrentPathIndex < CurrentPath.Num() - 1)
            {
                CurrentPathIndex++;

                GEngine->AddOnScreenDebugMessage(
                    -1,
                    1.f,
                    FColor::Orange,
                    TEXT("Snagged! Skipping node.")
                );
            }
        }
    }
    else
    {
        // Gradually clear stuck state while moving
        StuckTimer = FMath::FInterpTo(StuckTimer, 0.0f, DeltaTime, 2.0f);
    }

    // Stop if path is finished or was cleared
    if (CurrentPathIndex >= CurrentPath.Num()) return;

    // Advance waypoints early for smoother turns
    float WaypointTolerance = 120.0f;

    while (CurrentPathIndex < CurrentPath.Num() - 1 &&
           FVector::Distance(PawnLocation, CurrentPath[CurrentPathIndex]) < WaypointTolerance)
    {
        CurrentPathIndex++;
    }

    // Stop if path became complete
    if (CurrentPathIndex >= CurrentPath.Num()) return;

    FVector TargetPoint = CurrentPath[CurrentPathIndex];

    // Direction towards current waypoint
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
        float AvoidanceRadius = 1000.0f; 

        if (DistToZombie < AvoidanceRadius)
        {
            // Direction away from zombie
            FVector DirToZombie = (Zombie->GetActorLocation() - PawnLocation).GetSafeNormal();
            FVector PushAway = -DirToZombie;

            // Sideways vector to steer around zombie
            FVector Tangent = FVector::CrossProduct(DirToZombie, FVector::UpVector);

            // Pick side works best for destination
            if (FVector::DotProduct(Tangent, SeekForce) < 0)
            {
                Tangent = -Tangent; 
            }

            // Blend avoidance and sidestepping
            FVector CurveVector = (PushAway * 0.4f) + (Tangent * 0.6f);
            CurveVector.Normalize();

            // Stronger avoidance when closer to zombie
            float PushFactor = (AvoidanceRadius - DistToZombie) / AvoidanceRadius; 
            CurveVector *= (PushFactor * PushFactor); 

            SeparationForce += CurveVector;
            AvoidCount++;
        }
    }

    // Average avoidance forces
    if (AvoidCount > 0)
    {
        SeparationForce /= AvoidCount;
    }

    SeparationForce.Z = 0.0f;

    // Blend path 
    float SeekWeight = 1.0f;
    float SeparationWeight = 3.5f; 

    FVector DesiredDirection = (SeekForce * SeekWeight) + (SeparationForce * SeparationWeight);

    DesiredDirection.Z = 0.0f;
    DesiredDirection.Normalize();
    
    // Smooth movement direction over time
    CurrentVelocity = FMath::VInterpTo(CurrentVelocity, DesiredDirection, DeltaTime, 6.0f);

    // Apply movement
    SurvivorPawn->AddMovementInput(CurrentVelocity, 1.0f);
    
    // Rotate towards movement direction
    if (!CurrentVelocity.IsNearlyZero(0.1f))
    {
        float DirectionDot = FVector::DotProduct(SurvivorPawn->GetActorForwardVector(), CurrentVelocity);
       
        // Rotate faster during sharp turns
        float DynamicRotSpeed = FMath::GetMappedRangeValueClamped(FVector2D(1.0f, -1.0f), FVector2D(15.0f, 45.0f), DirectionDot);
       
        FRotator TargetRot = CurrentVelocity.Rotation();
        FRotator SmoothRot = FMath::RInterpTo(SurvivorPawn->GetActorRotation(), TargetRot, DeltaTime, DynamicRotSpeed);

        SurvivorPawn->SetActorRotation(SmoothRot);
    }

    // Use smaller tolerance for final waypoint
    float ArrivalTolerance = (CurrentPathIndex == CurrentPath.Num() - 1) ? 50.0f : 100.0f;
    
    // Advance to next waypoint
    if (FVector::Distance(PawnLocation, TargetPoint) < ArrivalTolerance)
    {
        CurrentPathIndex++; 
    }
}

bool USurvivorFSMDeroyThomas::HasUsableWeapon(int& OutSlotIndex)
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

void USurvivorFSMDeroyThomas::OnPurgeZoneSpotted(AActor* PurgeZone)
{
    ActivePurgeZone = PurgeZone;
}

bool USurvivorFSMDeroyThomas::GetBestWeapon(float TargetDistance, int& OutSlotIndex)
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

void USurvivorFSMDeroyThomas::OnZombieSpotted(AActor* Zombie)
{
    if (IsValid(ActivePurgeZone))
    {
        float DistToPurge = FVector::Distance(SurvivorPawn->GetActorLocation(), ActivePurgeZone->GetActorLocation());
        if (DistToPurge < 800.0f) return; 
    }
    
    // Zombie is far
    float DistToZombie = FVector::Distance(SurvivorPawn->GetActorLocation(), Zombie->GetActorLocation());
    bool bIsHeavy = Zombie->GetClass()->GetName().Contains("Heavy");
    
    if (bIsHeavy && DistToZombie > 250.0f) return;
    if (!bIsHeavy && DistToZombie > 450.0f) return;
    
    CurrentThreat = Zombie;

    int WeaponSlot = -1;
    
    // Try to find a gun close enough to get 
    if (!HasUsableWeapon(WeaponSlot))
    {
        AActor* BestGun = GetClosestKnownItem(EItemType::Shotgun);
        if (!BestGun) BestGun = GetClosestKnownItem(EItemType::Pistol);
        
        if (BestGun)
        {
            float DistToGun = FVector::Distance(SurvivorPawn->GetActorLocation(), BestGun->GetActorLocation());
            if (DistToGun < 800.0f) 
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, TEXT("Saw a gun! Diving for it!"));
                TargetItem = Cast<ABaseItem>(BestGun);
                ChangeState(UScavengeStateDeroyThomas::StaticClass());
                return;
            }
        }
    }
    
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
    
    // Safe ammo bc big boy
    if (bIsHeavy)
    {
        ChangeState(UFleeStateDeroyThomas::StaticClass());
        return;
    }
    
    // Use weapon
    if (GetBestWeapon(DistToZombie, WeaponSlot) && HealthPct > 0.4)
    {
        ChangeState(UCombatStateDeroyThomas::StaticClass());
    }
    // Hide in house
    else if (bHasValidHouse && DistToZombie > 1000.0f)
    {
        ChangeState(UHideStateDeroyThomas::StaticClass());
    }
    // Run
    else
    {
        ChangeState(UFleeStateDeroyThomas::StaticClass()); 
    }
}

void USurvivorFSMDeroyThomas::EvaluateInventory()
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

void USurvivorFSMDeroyThomas::OnDamageSensed(FVector DamageLocation)
{
    bIsReflexSpinning = true;
    
    // Check where the damage comes from
    if (!DamageLocation.IsNearlyZero())
    {
        ReflexTargetLocation = DamageLocation;
    }
    else
    {
        ReflexTargetLocation = SurvivorPawn->GetActorLocation() - (SurvivorPawn->GetActorForwardVector() * 500.0f);
    }
    
    GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Ouch! Whipping around to attacker!"));
}

AActor* USurvivorFSMDeroyThomas::GetClosestKnownItem(EItemType DesiredType)
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