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
#include "Kismet/GameplayStatics.h"
#include "States/ReturnState.h"
#include "States/ScavengeState.h"

USurvivorFSM::USurvivorFSM()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void USurvivorFSM::BeginPlay()
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
    
    ChangeState(UExploreState::StaticClass());
}

void USurvivorFSM::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!SurvivorPawn) return;
    
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
        DrawDebugCone(GetWorld(), PawnLoc, Forward, 500.0f, VisionAngleWidth, VisionAngleHeight, 24, FColor::Yellow, false, -1.0f, 0, 1.5f);

        // Draw house outline
        for (AHouse* House : AllLevelHouses)
        {
            if (!IsValid(House)) continue;

            FColor HouseColor = FColor::White; // Unknown

            if (VisitedHouses.Contains(House)) 
            {
                HouseColor = FColor::Black; // Visited
            }
            else if (KnownHouses.Contains(House)) 
            {
                HouseColor = FColor::Blue; // Known
            }
            
            FHouseBounds Bounds = House->GetBounds();
            DrawDebugBox(GetWorld(), Bounds.Origin, Bounds.Extent, FQuat::Identity, HouseColor, false, -1.0f, 0, 8.0f);
        }
        
        // Draw house outline & Spider Web
        for (AHouse* House : AllLevelHouses)
        {
            if (!IsValid(House)) continue;

            FColor HouseColor = FColor::White; // Unknown
            bool bIsTracked = false;
            FString HouseTag = TEXT("");

            if (VisitedHouses.Contains(House)) 
            {
                HouseColor = FColor::Black; // Visited
                bIsTracked = true;
                HouseTag = TEXT("Visited");
            }
            else if (KnownHouses.Contains(House)) 
            {
                HouseColor = FColor::Blue; // Known
                bIsTracked = true;
                HouseTag = TEXT("Known");
            }
            
            FHouseBounds Bounds = House->GetBounds();
            DrawDebugBox(GetWorld(), Bounds.Origin, Bounds.Extent, FQuat::Identity, HouseColor, false, -1.0f, 0, 8.0f);
            
            if (bIsTracked)
            {
                // Get id assigned when see
                int32 HouseID = DiscoveredHouseIDs.Contains(House) ? DiscoveredHouseIDs[House] : 0;
                
                // Draw name
                FVector TextLoc = House->GetActorLocation() + FVector(0.0f, 0.0f, 400.0f);
                FString HouseName = FString::Printf(TEXT("%s House %d"), *HouseTag, HouseID);
                DrawDebugString(GetWorld(), TextLoc, HouseName, nullptr, HouseColor, 0.0f, true, 1.5f);

                // Draw line
                DrawDebugLine(GetWorld(), PawnLoc, House->GetActorLocation(), HouseColor, false, -1.0f, 0, 1.5f);

                // Draw distance
                FVector Midpoint = PawnLoc + ((House->GetActorLocation() - PawnLoc) * 0.5f);
                float Dist = FVector::Distance(PawnLoc, House->GetActorLocation());
                FString DistText = FString::Printf(TEXT("%.0f"), Dist);
                DrawDebugString(GetWorld(), Midpoint, DistText, nullptr, HouseColor, 0.0f, true, 1.0f);
            }
        }

        // Draw zombie outline
        for (AActor* Zombie : KnownZombies) 
        {
            if (!IsValid(Zombie)) continue;

            FColor ZombieColor = FColor::Yellow; // Known by player

            if (Zombie == CurrentThreat) 
            {
                ZombieColor = FColor::Red; // Targeted with weapon
                
                // Draw a laser
                DrawDebugLine(GetWorld(), PawnLoc, Zombie->GetActorLocation(), FColor::Red, false, -1.0f, 0, 2.0f);
            }
            
            DrawDebugCapsule(GetWorld(), Zombie->GetActorLocation(), 90.0f, 40.0f, FQuat::Identity, ZombieColor, false, -1.0f, 0, 5.0f);
        }
        
        // Draw item outline
        TArray<AActor*> DynamicFoundItems;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABaseItem::StaticClass(), DynamicFoundItems);

        for (int32 i = DynamicFoundItems.Num() - 1; i >= 0; --i)
        {
            ABaseItem* Item = Cast<ABaseItem>(DynamicFoundItems[i]);
            if (!Item) continue;

            // Skip if it has an owner
            if (Item->GetOwner() != nullptr) continue;
            
            // Skip our items in the pocket
            UInventoryComponent* Inv = SurvivorPawn->GetComponentByClass<UInventoryComponent>();
            if (Inv && Inv->GetInventory().Contains(Item)) continue;
            
            // Skip garbage 
            if (Item->GetItemType() == EItemType::Garbage) continue; 

            FColor ItemColor = FColor::White; // Unknown item
            bool bIsKnown = KnownItems.Contains(Item);

            if (bIsKnown)
            {
                ItemColor = FColor::Cyan; // Known item
                
                DrawDebugLine(GetWorld(), PawnLoc, Item->GetActorLocation(), FColor::Cyan, false, -1.0f, 0, 1.0f);
                
                FString ItemName = TEXT("Item");
                switch (Item->GetItemType())
                {
                case EItemType::Pistol:   ItemName = TEXT("Pistol"); break;
                case EItemType::Shotgun:  ItemName = TEXT("Shotgun"); break;
                case EItemType::Medkit:   ItemName = TEXT("Medkit"); break;
                case EItemType::Food:     ItemName = TEXT("Food"); break;
                }
                
                FVector TextLoc = Item->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
                DrawDebugString(GetWorld(), TextLoc, ItemName, nullptr, FColor::Cyan, 0.0f, true, 1.0f);
            }
            
            DrawDebugBox(GetWorld(), Item->GetActorLocation(), FVector(15.0f, 15.0f, 15.0f), FQuat::Identity, ItemColor, false, -1.0f, 0, 3.0f);
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
            DebugText += FString::Printf(TEXT("\nWANDER TIMER: %.1f"), TimeSinceLastTown);
        }

        if (CurrentThreat) DebugText += TEXT("\nTargeting: ZOMBIE RUN");
        if (ActivePurgeZone) DebugText += TEXT("\nTargeting: PURGE ZONE RUN");
        
        DrawDebugString(GetWorld(), HeadLoc, DebugText, nullptr, FColor::Cyan, 0.0f, true, 1.2f);
    }
    
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
            OnDamageSensed(FVector::ZeroVector);
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
    
    // Check if zombies are too close
    if (CurrentState && (CurrentState->GetClass() == UExploreState::StaticClass() || 
                         CurrentState->GetClass() == UReturnState::StaticClass() || 
                         CurrentState->GetClass() == UScavengeState::StaticClass()))
    {
        for (AActor* Zombie : KnownZombies)
        {
            if (!IsValid(Zombie)) continue;
            
            float Dist = FVector::Distance(SurvivorPawn->GetActorLocation(), Zombie->GetActorLocation());
            bool bIsHeavy = Zombie->GetClass()->GetName().Contains("Heavy");

            // If zombie too close we react
            if ((bIsHeavy && Dist < 250.0f) || (!bIsHeavy && Dist < 450.0f))
            {
                OnZombieSpotted(Zombie); 
                break; 
            }
        }
    }
    
    if (CurrentState)
    {
        CurrentState->Update(DeltaTime);
    }
    


}

void USurvivorFSM::ChangeState(TSubclassOf<UBaseState> NewStateClass)
{
    if (CurrentState)
    {
        if (CurrentState->GetClass() == NewStateClass) return; 
        
        UClass* CurrentClass = CurrentState->GetClass();
        if (CurrentClass != UFleeState::StaticClass() && 
            CurrentClass != UCombatState::StaticClass() && 
            CurrentClass != UHideState::StaticClass())
        {
            PreviousStateClass = CurrentClass;
        }
        
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

void USurvivorFSM::ResumePreviousState()
{
    if (PreviousStateClass)
    {
        ChangeState(PreviousStateClass);
    }
    else
    {
        ChangeState(UExploreState::StaticClass());
    }
}

void USurvivorFSM::MoveAlongPath(float DeltaTime)
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
        FRotator TargetRot = CurrentVelocity.Rotation();
       
        FRotator SmoothRot = FMath::RInterpTo(
            SurvivorPawn->GetActorRotation(),
            TargetRot,
            DeltaTime,
            10.0f
        );

        SurvivorPawn->SetActorRotation(SmoothRot);
    }

    // Use smaller tolerance for final waypoint
    float ArrivalTolerance = (CurrentPathIndex == CurrentPath.Num() - 1) ? 50.0f : 100.0f;

    // Advance to next waypoint
    if (FVector::Distance(PawnLocation, TargetPoint) < ArrivalTolerance)
    {
        CurrentPathIndex++; 
    }

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
    if (ActivePurgeZone) return;
    
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
                ChangeState(UScavengeState::StaticClass());
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
        ChangeState(UFleeState::StaticClass());
        return;
    }
    
    // Use weapon
    if (GetBestWeapon(DistToZombie, WeaponSlot) && HealthPct > 0.4)
    {
        ChangeState(UCombatState::StaticClass());
    }
    // Hide in house
    else if (bHasValidHouse && DistToZombie > 1000.0f)
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