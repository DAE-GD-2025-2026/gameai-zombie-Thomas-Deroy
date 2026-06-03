#include "FleeStateDeroyThomas.h"
#include "CombatStateDeroyThomas.h"
#include "ExploreStateDeroyThomas.h"
#include "HideStateDeroyThomas.h"
#include "../SurvivorFSMDeroyThomas.h"
#include "Survivor/SurvivorPawn.h"
#include "Common/StaminaComponent.h"

void UFleeStateDeroyThomas::Enter(USurvivorFSMDeroyThomas* FSM)
{
    Super::Enter(FSM);
    GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("State Changed To: Flee"));

    // Reset
    bIsCheckingBehind = false;
    CheckTimer = 0.0f;
    FleeTime = 0.0f;

    if (ContextFSM && ContextFSM->SurvivorPawn)
    {
        ContextFSM->SurvivorPawn->StartRunning();
        ContextFSM->CurrentPath.Empty();
    }
}

void UFleeStateDeroyThomas::Update(float DeltaTime)
{
    Super::Update(DeltaTime);
    if (!ContextFSM || !ContextFSM->SurvivorPawn) return;

    static FVector LastKnownThreatLoc = FVector::ZeroVector;
    
    // Pick main threat 
    AActor* ThreatToEvade = nullptr;
    if (IsValid(ContextFSM->ActivePurgeZone)) 
    {
        ThreatToEvade = ContextFSM->ActivePurgeZone;
    }
    else if (IsValid(ContextFSM->CurrentThreat)) 
    {
        ThreatToEvade = ContextFSM->CurrentThreat;
    }
    
    if (ThreatToEvade) LastKnownThreatLoc = ThreatToEvade->GetActorLocation();

    FVector PawnLoc = ContextFSM->SurvivorPawn->GetActorLocation();

    // Look back
    if (HandleShoulderCheck(DeltaTime, PawnLoc, ThreatToEvade, LastKnownThreatLoc)) return;

    // No threat anymore
    if (!ThreatToEvade)
    {
        ContextFSM->ResumePreviousState();
        return;
    }

    if (!bIsCheckingBehind) FleeTime += DeltaTime;

    float DistToThreat = FVector::Distance(PawnLoc, ThreatToEvade->GetActorLocation());
    bool bIsPurge = (ThreatToEvade == ContextFSM->ActivePurgeZone);

    UpdateSafetyAndStamina(DistToThreat, bIsPurge);
    if (bIsCheckingBehind) return;

    // Move away
    if (ContextFSM->CurrentPath.IsEmpty() || ContextFSM->CurrentPathIndex >= ContextFSM->CurrentPath.Num())
    {
        GenerateEscapeRoute(DeltaTime, PawnLoc, ThreatToEvade, bIsPurge, DistToThreat);
    }
    else
    {
        ContextFSM->MoveAlongPath(DeltaTime);
    }
}

void UFleeStateDeroyThomas::Exit()
{
    Super::Exit();

    // cleanup flee state
    if (ContextFSM && ContextFSM->SurvivorPawn)
    {
        ContextFSM->SurvivorPawn->StopRunning();
        ContextFSM->CurrentPath.Empty();
    }
}

bool UFleeStateDeroyThomas::HandleShoulderCheck(float DeltaTime, FVector PawnLoc, AActor* ThreatToEvade, FVector LastKnownThreatLoc)
{
    if (!bIsCheckingBehind) return false;

    CheckTimer += DeltaTime;

    // Run away from last known threat 
    FVector RunDir = (PawnLoc - LastKnownThreatLoc).GetSafeNormal();
    RunDir.Z = 0.0f;

    ContextFSM->SurvivorPawn->AddMovementInput(RunDir, 1.0f);

    // Look back while running
    FRotator SmoothRot = FMath::RInterpTo(ContextFSM->SurvivorPawn->GetActorRotation(),(-RunDir).Rotation(), DeltaTime, 20.0f);

    ContextFSM->SurvivorPawn->SetActorRotation(SmoothRot);

    // Threat is back 
    if (ThreatToEvade && ThreatToEvade != ContextFSM->ActivePurgeZone)
    {
        bIsCheckingBehind = false;
        CheckTimer = 0.0f;
        FleeTime = 0.0f;

        float DistToThreat = FVector::Distance(PawnLoc, ThreatToEvade->GetActorLocation());
        int HordeSize = ContextFSM->KnownZombies.Num();

        // Group decision: Fight / Hide / Keep running
        if (HordeSize >= 3)
        {
            int WSlot;
            if (ContextFSM->GetBestWeapon(DistToThreat, WSlot))
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Horde! Opening fire!"));
                ContextFSM->ChangeState(UCombatStateDeroyThomas::StaticClass());
                return true;
            }

            bool bHasHouse = false;
            for (AHouse* H : ContextFSM->KnownHouses)
                if (IsValid(H)) { bHasHouse = true; break; }

            if (!bHasHouse)
                for (AHouse* H : ContextFSM->VisitedHouses)
                    if (IsValid(H)) { bHasHouse = true; break; }

            if (bHasHouse)
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, TEXT("No ammo -> hide!"));
                ContextFSM->ChangeState(UHideStateDeroyThomas::StaticClass());
                return true;
            }

            GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("No options -> keep running"));
        }
        else
        {
            int WSlot;
            if (DistToThreat > 600.0f && ContextFSM->GetBestWeapon(DistToThreat, WSlot))
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("Fight back!"));
                ContextFSM->ChangeState(UCombatStateDeroyThomas::StaticClass());
                return true;
            }
        }

        return true;
    }

    // Calm down after timeout
    if (CheckTimer > 1.5f)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("Lost them"));

        FVector Dir = (PawnLoc - LastKnownThreatLoc).GetSafeNormal();
        Dir.Z = 0.0f;

        ContextFSM->SurvivorPawn->SetActorRotation(Dir.Rotation());
        ContextFSM->ResumePreviousState();
    }

    return true;
}

void UFleeStateDeroyThomas::UpdateSafetyAndStamina(float DistToThreat, bool bIsPurge)
{
    float SafeDistance = bIsPurge ? 2000.0f : 2100.0f;

    // When to start lookback
    if ((FleeTime > 5.0f || DistToThreat > SafeDistance) && !bIsCheckingBehind)
    {
        if (bIsPurge && DistToThreat > SafeDistance)
        {
            ContextFSM->ActivePurgeZone = nullptr;
        }
        else if (!bIsPurge)
        {
            ContextFSM->CurrentThreat = nullptr;
        }

        bIsCheckingBehind = true;
        CheckTimer = 0.0f;
        ContextFSM->CurrentPath.Empty();
    }

    if (bIsCheckingBehind) return;

    // Stamina check
    UStaminaComponent* StaminaComp = ContextFSM->SurvivorPawn->GetComponentByClass<UStaminaComponent>();

    float StaminaPct = StaminaComp ? (StaminaComp->GetCurrentStamina() / StaminaComp->GetMaxStamina()) : 1.0f;

    // Close range panic fight
    if (!bIsPurge && DistToThreat < 250.0f)
    {
        int WSlot;
        if (ContextFSM->GetBestWeapon(DistToThreat, WSlot))
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Last resort fight"));
            ContextFSM->ChangeState(UCombatStateDeroyThomas::StaticClass());
            return;
        }
    }

    bool bIsRunner = IsValid(ContextFSM->CurrentThreat) && ContextFSM->CurrentThreat->GetClass()->GetName().Contains("Runner");
    bool bIsDangerClose = DistToThreat < 500.0f;

    // Run condition
    if ((bIsPurge || bIsRunner || bIsDangerClose) && StaminaPct > 0.15f)
        ContextFSM->SurvivorPawn->StartRunning();
    else
        ContextFSM->SurvivorPawn->StopRunning();
}

void UFleeStateDeroyThomas::GenerateEscapeRoute(float DeltaTime, FVector PawnLoc, AActor* ThreatToEvade, bool bIsPurge, float DistToThreat)
{
    FVector DangerCenter = FVector::ZeroVector;
    int Count = 0;

    if (bIsPurge)
    {
        DangerCenter = ThreatToEvade->GetActorLocation();
        Count = 1;
    }
    else
    {
        for (AActor* Zombie : ContextFSM->KnownZombies)
        {
            if (IsValid(Zombie))
            {
                DangerCenter += Zombie->GetActorLocation();
                Count++;
            }
        }
    }

    DangerCenter = (Count > 0)
        ? (DangerCenter / Count)
        : ThreatToEvade->GetActorLocation();

    FVector RunDir = (PawnLoc - DangerCenter).GetSafeNormal();
    RunDir.Z = 0.0f;

    FVector EscapeLocation = PawnLoc + (RunDir * 1200.0f);

    // If unarmed prefer known house
    int DummySlot;
    if (!bIsPurge && !ContextFSM->HasUsableWeapon(DummySlot))
    {
        AHouse* BestHouse = nullptr;
        float BestDist = FLT_MAX;

        TArray<AHouse*> AllHouses = ContextFSM->KnownHouses;
        AllHouses.Append(ContextFSM->VisitedHouses);

        for (AHouse* House : AllHouses)
        {
            if (!IsValid(House)) continue;

            FVector DirToHouse = (House->GetActorLocation() - PawnLoc).GetSafeNormal();

            if (FVector::DotProduct(RunDir, DirToHouse) > -0.3f)
            {
                float Dist = FVector::Distance(PawnLoc, House->GetActorLocation());
                if (Dist < BestDist)
                {
                    BestDist = Dist;
                    BestHouse = House;
                }
            }
        }

        if (BestHouse)
        {
            EscapeLocation = BestHouse->GetBounds().Origin;
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("Running to house"));
        }
    }

    ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(EscapeLocation);
    ContextFSM->CurrentPathIndex = 0;

    // If path fails
    if (ContextFSM->CurrentPath.IsEmpty())
    {
        int WSlot = -1;

        if (!bIsPurge && ContextFSM->GetBestWeapon(DistToThreat, WSlot))
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Cornered fight"));
            ContextFSM->ChangeState(UCombatStateDeroyThomas::StaticClass());
        }
        else
        {
            ContextFSM->SurvivorPawn->AddMovementInput(RunDir, 1.0f);

            FRotator Rot =
                FMath::RInterpTo(ContextFSM->SurvivorPawn->GetActorRotation(),
                                 RunDir.Rotation(),
                                 DeltaTime, 15.0f);

            ContextFSM->SurvivorPawn->SetActorRotation(Rot);
        }
    }
}