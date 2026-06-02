#include "FleeState.h"

#include "CombatState.h"
#include "ExploreState.h"
#include "HideState.h"
#include "../SurvivorFSM.h"
#include "Survivor/SurvivorPawn.h"

void UFleeState::Enter(USurvivorFSM* FSM)
{
    Super::Enter(FSM);
    GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("State Changed To: Flee"));

    bIsCheckingBehind = false;
    CheckTimer = 0.0f;
    FleeTime = 0.0f;
    
    if (ContextFSM && ContextFSM->SurvivorPawn)
    {
        ContextFSM->SurvivorPawn->StartRunning(); 
        ContextFSM->CurrentPath.Empty(); 
    }
}

void UFleeState::Update(float DeltaTime)
{
    Super::Update(DeltaTime);

    // Validate FSM and pawn
    if (!ContextFSM || !ContextFSM->SurvivorPawn) return;

    // Track the current threat
    static FVector LastKnownThreatLoc = FVector::ZeroVector;

    AActor* ThreatToEvade = ContextFSM->ActivePurgeZone ? ContextFSM->ActivePurgeZone : ContextFSM->CurrentThreat;

    if (ThreatToEvade)
    {
        LastKnownThreatLoc = ThreatToEvade->GetActorLocation();
    }

    FVector PawnLoc = ContextFSM->SurvivorPawn->GetActorLocation();

    // Shoulder check phase after escaping
    if (bIsCheckingBehind)
    {
        CheckTimer += DeltaTime;

        // Keep moving away from last known threat location
        FVector RunDir = (PawnLoc - LastKnownThreatLoc).GetSafeNormal();
        RunDir.Z = 0.0f;

        ContextFSM->SurvivorPawn->AddMovementInput(RunDir, 1.0f);

        // Turn around to look behind
        FRotator LookBackRot = (-RunDir).Rotation();

        FRotator SmoothRot = FMath::RInterpTo(
            ContextFSM->SurvivorPawn->GetActorRotation(),
            LookBackRot,
            DeltaTime,
            20.0f
        );

        ContextFSM->SurvivorPawn->SetActorRotation(SmoothRot);

        // Threat spotted again
        if (ThreatToEvade && ThreatToEvade != ContextFSM->ActivePurgeZone)
        {
            bIsCheckingBehind = false;
            CheckTimer = 0.0f;
            FleeTime = 0.0f;
            
            float DistToThreat = FVector::Distance(PawnLoc, ThreatToEvade->GetActorLocation());
            int HordeSize = ContextFSM->KnownZombies.Num();

            // Handle large zombie groups
            if (HordeSize >= 3)
            {
                int WSlot;
                
                // We use a gun
                if (ContextFSM->GetBestWeapon(DistToThreat, WSlot)) 
                {
                    GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Horde! Opening fire to thin them out!"));
                    ContextFSM->ChangeState(UCombatState::StaticClass());
                    return;
                }
                
                // Search for known house to hide
                bool bHasValidHouse = false;
                for (AHouse* H : ContextFSM->KnownHouses) if (IsValid(H)) { bHasValidHouse = true; break; }
                if (!bHasValidHouse) for (AHouse* H : ContextFSM->VisitedHouses) if (IsValid(H)) { bHasValidHouse = true; break; }

                if (bHasValidHouse) 
                {
                    GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, TEXT("Out of ammo! Diving into a house!"));
                    ContextFSM->ChangeState(UHideState::StaticClass());
                    return;
                } 
                
                // No ammo or known house
                GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("No ammo and no safety! Run!"));
                return;
            }
            else
            {
                // Fight isolated zombies from a safe distance
                int WSlot;

                if (DistToThreat > 600.0f && ContextFSM->GetBestWeapon(DistToThreat, WSlot))
                {
                    GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("Got the drop on them! Opening fire!"));

                    ContextFSM->ChangeState(UCombatState::StaticClass());

                    return;
                }
            }

            // Continue fleeing
            GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("Too close/No ammo! Keep running!"));

            return;
        }

        // Threat successfully lost
        if (CheckTimer > 1.5f)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("Lost them. Back to exploring."));

            FVector OriginalDir = (PawnLoc - LastKnownThreatLoc).GetSafeNormal();
            OriginalDir.Z = 0.0f;
            ContextFSM->SurvivorPawn->SetActorRotation(OriginalDir.Rotation());

            ContextFSM->ResumePreviousState();
        }

        return;
    }

    // Return to exploring if no threat exists
    if (!ThreatToEvade)
    {
        ContextFSM->ResumePreviousState();

        return;
    }
    
    if (!bIsCheckingBehind)
    {
        FleeTime += DeltaTime;
    }

    float DistToThreat = FVector::Distance(PawnLoc, ThreatToEvade->GetActorLocation());

    float SafeDistance = (ThreatToEvade == ContextFSM->ActivePurgeZone) ? 2000.0f : 2100.0f;

    // Start shoulder check when considered safe
    if ((FleeTime > 5.0f || DistToThreat > SafeDistance) && !bIsCheckingBehind)
    {
        if (ThreatToEvade == ContextFSM->ActivePurgeZone)
        {
            // If threat is too close run
            if (DistToThreat > SafeDistance)
            {
                ContextFSM->ActivePurgeZone = nullptr;
                bIsCheckingBehind = true;
                CheckTimer = 0.0f;
                ContextFSM->CurrentPath.Empty();
                return;
            }
        }
        else
        {
            // No threat we safe
            ContextFSM->CurrentThreat = nullptr;
            bIsCheckingBehind = true;
            CheckTimer = 0.0f;
            ContextFSM->CurrentPath.Empty();
            return;
        }
    }

    // Manage stamina while fleeing
    UStaminaComponent* StaminaComp = ContextFSM->SurvivorPawn->GetComponentByClass<UStaminaComponent>();

    float StaminaPct = StaminaComp ? (StaminaComp->GetCurrentStamina() / StaminaComp->GetMaxStamina()) : 1.0f;

    bool bIsPurge = (ThreatToEvade == ContextFSM->ActivePurgeZone);
    
    if (!bIsPurge && DistToThreat < 250.0f)
    {
        int WSlot;
        if (ContextFSM->GetBestWeapon(DistToThreat, WSlot))
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Fuck it american way"));
            ContextFSM->ChangeState(UCombatState::StaticClass());
            return;
        }
    }
    
    bool bIsRunner = ThreatToEvade->GetClass()->GetName().Contains("Runner");
    bool bIsDangerClose = DistToThreat < 500.0f;

    if ((bIsPurge || bIsRunner || bIsDangerClose) && StaminaPct > 0.15f)
    {
        ContextFSM->SurvivorPawn->StartRunning();
    }
    else
    {
        ContextFSM->SurvivorPawn->StopRunning();
    }

    // Generate escape route if needed
    if (ContextFSM->CurrentPath.IsEmpty() || ContextFSM->CurrentPathIndex >= ContextFSM->CurrentPath.Num())
    {
        FVector DangerCenter = FVector::ZeroVector;
        int ThreatCount = 0;

        if (bIsPurge)
        {
            DangerCenter = ThreatToEvade->GetActorLocation();
            ThreatCount = 1;
        }
        else
        {
            // Calculate center of nearby zombie group
            for (AActor* Zombie : ContextFSM->KnownZombies)
            {
                if (IsValid(Zombie))
                {
                    DangerCenter += Zombie->GetActorLocation();
                    ThreatCount++;
                }
            }
        }

        if (ThreatCount > 0)
        {
            DangerCenter /= ThreatCount;
        }
        else
        {
            DangerCenter = ThreatToEvade->GetActorLocation();
        }

        FVector RunDir = (PawnLoc - DangerCenter).GetSafeNormal();
        RunDir.Z = 0.0f;

        FVector EscapeLocation = PawnLoc + (RunDir * 1200.0f);

        // Look for shelter if unarmed
        int DummySlot;

        // Search for houses if no weapons
        if (!bIsPurge && !ContextFSM->HasUsableWeapon(DummySlot))
        {
            AHouse* BestHouse = nullptr;
            float ClosestDist = FLT_MAX;
            
            TArray<AHouse*> AllRememberedHouses = ContextFSM->KnownHouses;
            AllRememberedHouses.Append(ContextFSM->VisitedHouses);

            for (AHouse* House : AllRememberedHouses)
            {
                if (IsValid(House))
                {
                    FVector DirToHouse = (House->GetActorLocation() - PawnLoc).GetSafeNormal();
            
                    // Ignore houses that has zombies blocking it
                    if (FVector::DotProduct(RunDir, DirToHouse) > -0.3f)
                    {
                        float DistToHouse = FVector::Distance(PawnLoc, House->GetActorLocation());

                        if (DistToHouse < ClosestDist)
                        {
                            ClosestDist = DistToHouse;
                            BestHouse = House;
                        }
                    }
                }
            }

            // Use nearest safe house as escape target
            if (BestHouse)
            {
                EscapeLocation = BestHouse->GetBounds().Origin;

                GEngine->AddOnScreenDebugMessage(
                    -1,
                    3.f,
                    FColor::Yellow,
                    TEXT("Defenseless! Scrambling to nearest house for weapons!")
                );
            }
        }
        
        ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(EscapeLocation);
        ContextFSM->CurrentPathIndex = 0;

        // Handle failed pathfinding
        if (ContextFSM->CurrentPath.IsEmpty())
        {
            int WSlot = -1;

            if (!bIsPurge && ContextFSM->GetBestWeapon(DistToThreat, WSlot))
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Cornered! Fighting for my life!"));

                ContextFSM->ChangeState(UCombatState::StaticClass());

                return;
            }
            else
            {
                ContextFSM->SurvivorPawn->AddMovementInput(RunDir, 1.0f);

                FRotator SmoothRot = FMath::RInterpTo(
                    ContextFSM->SurvivorPawn->GetActorRotation(),
                    RunDir.Rotation(),
                    DeltaTime,
                    15.0f
                );

                ContextFSM->SurvivorPawn->SetActorRotation(SmoothRot);

                return;
            }
        }
    }

    // Follow escape path
    ContextFSM->MoveAlongPath(DeltaTime);
}

void UFleeState::Exit()
{
    Super::Exit();
    if (ContextFSM && ContextFSM->SurvivorPawn)
    {
        ContextFSM->SurvivorPawn->StopRunning(); 
        ContextFSM->CurrentPath.Empty();
    }
}