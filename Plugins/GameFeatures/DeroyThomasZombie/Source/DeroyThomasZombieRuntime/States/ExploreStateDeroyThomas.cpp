#include "ExploreStateDeroyThomas.h"
#include "ReturnStateDeroyThomas.h"
#include "ScavengeStateDeroyThomas.h"
#include "../SurvivorFSMDeroyThomas.h"
#include "Common/InventoryComponent.h"
#include "Common/HealthComponent.h"
#include "Common/StaminaComponent.h"
#include "Survivor/SurvivorPawn.h"
#include "Village/House/House.h"

void UExploreStateDeroyThomas::Enter(USurvivorFSMDeroyThomas* FSM)
{
	Super::Enter(FSM);

    // Reset paranoia cycle
    ParanoiaTimer = 0.0f;
    TimeUntilNextCheck = FMath::RandRange(8.0f, 15.0f);
    bIsParanoiaChecking = false;
    
    if (ContextFSM && ContextFSM->SurvivorPawn) ContextFSM->SurvivorPawn->StopRunning();

	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, TEXT("State Changed To: Explore"));
}

void UExploreStateDeroyThomas::Update(float DeltaTime)
{
    Super::Update(DeltaTime);
    if (!ContextFSM || !ContextFSM->SurvivorPawn) return;

    if (HandleStartSpin(DeltaTime)) return;
    if (CheckSuppliesAndReturn(DeltaTime)) return;
    if (HandleHouseSearching()) return;
    
    if (ContextFSM->CurrentPath.IsEmpty() || ContextFSM->CurrentPathIndex >= ContextFSM->CurrentPath.Num())
    {
        // Reset paranoia state when path ends
        if (bIsParanoiaChecking)
        {
            bIsParanoiaChecking = false;
            ParanoiaTimer = 0.0f;
            TimeUntilNextCheck = FMath::RandRange(8.0f, 15.0f);
        }

        if (CheckDesperationMemory()) return;
        GenerateExplorationPath();
    }
    else
    {
        ContextFSM->MoveAlongPath(DeltaTime);
    }

    HandleParanoia(DeltaTime);
}

void UExploreStateDeroyThomas::Exit()
{
	Super::Exit();

    if (ContextFSM) ContextFSM->CurrentPath.Empty();
}

bool UExploreStateDeroyThomas::HandleStartSpin(float DeltaTime)
{
    if (!ContextFSM->bHasDoneStartSpin)
    {
        ContextFSM->SpinTimer += DeltaTime;

        FRotator CurrentRot = ContextFSM->SurvivorPawn->GetActorRotation();
        CurrentRot.Yaw += 180.0f * DeltaTime;
        ContextFSM->SurvivorPawn->SetActorRotation(CurrentRot);

        if (ContextFSM->SpinTimer >= 2.0f)
        {
            ContextFSM->bHasDoneStartSpin = true;
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, TEXT("Check spawn env"));
        }
        return true;
    }
    return false;
}

bool UExploreStateDeroyThomas::CheckSuppliesAndReturn(float DeltaTime)
{
    UInventoryComponent* Inventory = ContextFSM->SurvivorPawn->GetComponentByClass<UInventoryComponent>();
    int EmptySlots = 0;

    if (Inventory)
    {
        for (auto const& Item : Inventory->GetInventory())
            if (!Item) EmptySlots++;
    }

    // No known houses, but visited ones
    if (ContextFSM->KnownHouses.IsEmpty() && ContextFSM->VisitedHouses.Num() > 0)
    {
        ContextFSM->TimeSinceLastTown += DeltaTime;

        if (EmptySlots >= 3 && ContextFSM->TimeSinceLastTown > 20.0f)
        {
            ContextFSM->ChangeState(UReturnStateDeroyThomas::StaticClass());
            return true;
        }

        if (ContextFSM->TimeSinceLastTown > 40.0f)
        {
            ContextFSM->KnownHouses = ContextFSM->VisitedHouses;
            ContextFSM->VisitedHouses.Empty();
            ContextFSM->TimeSinceLastTown = 0.0f;

            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("Wandered enough. Checking old houses!"));
            ContextFSM->ChangeState(UReturnStateDeroyThomas::StaticClass());
            return true;
        }
    }
    else
    {
        ContextFSM->TimeSinceLastTown = 0.0f;
    }

    return false;
}

bool UExploreStateDeroyThomas::HandleHouseSearching()
{
    if (ContextFSM->TargetHouse)
    {
        float DistToHouse = FVector::Distance(ContextFSM->SurvivorPawn->GetActorLocation(), ContextFSM->TargetHouse->GetActorLocation());
        bool bReachedWaypoint = false;
        
        if (ContextFSM->CurrentPath.Num() > 0)
        {
            float DistToWaypoint = FVector::Distance(ContextFSM->SurvivorPawn->GetActorLocation(), ContextFSM->CurrentPath.Last());
            if (DistToWaypoint < 150.0f || ContextFSM->CurrentPathIndex >= ContextFSM->CurrentPath.Num())
            {
                bReachedWaypoint = true;
            }
        }
        
        if ((bReachedWaypoint || ContextFSM->CurrentPath.IsEmpty()) && DistToHouse < 800.0f)
        {
            if (SearchWaypoints > 0)
            {
                SearchWaypoints--;
                FHouseBounds Bounds = ContextFSM->TargetHouse->GetBounds();
                
                FVector RandomOffset;
                RandomOffset.X = FMath::RandRange(-Bounds.Extent.X * 0.4f, Bounds.Extent.X * 0.4f);
                RandomOffset.Y = FMath::RandRange(-Bounds.Extent.Y * 0.4f, Bounds.Extent.Y * 0.4f);
                RandomOffset.Z = 0.0f;

                ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(Bounds.Origin + RandomOffset);
                ContextFSM->CurrentPathIndex = 0;
                return true;
            }
            else
            {
                // Finished looting
                ContextFSM->VisitedHouses.Add(ContextFSM->TargetHouse);
                ContextFSM->KnownHouses.Remove(ContextFSM->TargetHouse);
                ContextFSM->TargetHouse = nullptr;
                ContextFSM->CurrentPath.Empty();
            }
        }
    }

    return false;
}

bool UExploreStateDeroyThomas::CheckDesperationMemory()
{
    // Low health/stamina -> search for needed items
    UHealthComponent* HealthComp = ContextFSM->SurvivorPawn->GetComponentByClass<UHealthComponent>();
    float HealthPct = HealthComp ? (float)HealthComp->GetHealth() / (float)HealthComp->GetMaxHealth() : 1.0f;

    UStaminaComponent* StamComp = ContextFSM->SurvivorPawn->GetComponentByClass<UStaminaComponent>();
    float StaminaPct = StamComp ? StamComp->GetCurrentStamina() / StamComp->GetMaxStamina() : 1.0f;

    UInventoryComponent* Inventory = ContextFSM->SurvivorPawn->GetComponentByClass<UInventoryComponent>();
    bool bHasMedkit = false, bHasFood = false;

    if (Inventory)
    {
        for (auto const& Item : Inventory->GetInventory())
        {
            if (!Item) continue;

            if (Item->GetItemType() == EItemType::Medkit) bHasMedkit = true;
            if (Item->GetItemType() == EItemType::Food) bHasFood = true;
        }
    }

    // Try find medkit if low hp
    if (HealthPct < 0.5f && !bHasMedkit)
    {
        if (AActor* Medkit = ContextFSM->GetClosestKnownItem(EItemType::Medkit))
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, TEXT("Desperation: Fetching Medkit!"));
            ContextFSM->TargetItem = Cast<ABaseItem>(Medkit);
            ContextFSM->ChangeState(UScavengeStateDeroyThomas::StaticClass());
            return true;
        }
    }
    // Try find food if low stamina
    else if (StaminaPct < 0.4f && !bHasFood)
    {
        if (AActor* Food = ContextFSM->GetClosestKnownItem(EItemType::Food))
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Orange, TEXT("Desperation: Fetching Food!"));
            ContextFSM->TargetItem = Cast<ABaseItem>(Food);
            ContextFSM->ChangeState(UScavengeStateDeroyThomas::StaticClass());
            return true;
        }
    }

    return false;
}

void UExploreStateDeroyThomas::GenerateExplorationPath()
{
    // Go to current target house if set
    if (ContextFSM->TargetHouse)
    {
        ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(ContextFSM->TargetHouse->GetBounds().Origin);

        ContextFSM->CurrentPathIndex = 0;

        if (!ContextFSM->CurrentPath.IsEmpty()) return;
        else { ContextFSM->TargetHouse = nullptr; }
        
        ContextFSM->TargetHouse = nullptr;
    }

    // Pick random known house
    if (ContextFSM->KnownHouses.Num() > 0 && !ContextFSM->TargetHouse)
    {
        int Index = FMath::RandRange(0, ContextFSM->KnownHouses.Num() - 1);
        ContextFSM->TargetHouse = ContextFSM->KnownHouses[Index];

        SearchWaypoints = 2;

        ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(ContextFSM->TargetHouse->GetBounds().Origin);

        ContextFSM->CurrentPathIndex = 0;

        if (!ContextFSM->CurrentPath.IsEmpty()) return;
        else { ContextFSM->TargetHouse = nullptr; }

        ContextFSM->VisitedHouses.AddUnique(ContextFSM->TargetHouse);
        ContextFSM->KnownHouses.Remove(ContextFSM->TargetHouse);
        ContextFSM->TargetHouse = nullptr;
    }

    // Random roam 
    FVector ForwardDir = ContextFSM->SurvivorPawn->GetVelocity().GetSafeNormal();
    if (ForwardDir.IsNearlyZero()) ForwardDir = ContextFSM->SurvivorPawn->GetActorForwardVector();

    float Angle = FMath::RandRange(-60.0f, 60.0f);
    FVector Dir = ForwardDir.RotateAngleAxis(Angle, FVector::UpVector);
    Dir.Z = 0.0f;
    Dir.Normalize();

    FVector Target = ContextFSM->SurvivorPawn->GetActorLocation() + (Dir * 1500.0f);

    ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(Target);

    ContextFSM->CurrentPathIndex = 0;
}

void UExploreStateDeroyThomas::HandleParanoia(float DeltaTime)
{
    if (bIsParanoiaChecking)
    {
        if (ParanoiaCheckDuration == 0.0f)
            CurrentParanoiaRot = ContextFSM->SurvivorPawn->GetActorRotation();

        ParanoiaCheckDuration += DeltaTime;

        FVector MoveDir = ContextFSM->SurvivorPawn->GetVelocity().GetSafeNormal();
        if (MoveDir.IsNearlyZero())
            MoveDir = ContextFSM->SurvivorPawn->GetActorForwardVector();

        FRotator LookBackRot = MoveDir.Rotation();
        LookBackRot.Yaw += ParanoiaAngle;
        LookBackRot.Normalize();

        CurrentParanoiaRot =
            FMath::RInterpTo(CurrentParanoiaRot, LookBackRot, DeltaTime, 15.0f);

        ContextFSM->SurvivorPawn->SetActorRotation(CurrentParanoiaRot);

        if (ParanoiaCheckDuration > 1.5f)
        {
            bIsParanoiaChecking = false;
            ParanoiaTimer = 0.0f;
            TimeUntilNextCheck = FMath::RandRange(8.0f, 15.0f);

            ContextFSM->SurvivorPawn->SetActorRotation(MoveDir.Rotation());
        }
    }
    else
    {
        ParanoiaTimer += DeltaTime;

        if (ParanoiaTimer >= TimeUntilNextCheck)
        {
            bIsParanoiaChecking = true;
            ParanoiaCheckDuration = 0.0f;
            ParanoiaAngle = FMath::RandRange(160.0f, 200.0f);
        }
    }
}