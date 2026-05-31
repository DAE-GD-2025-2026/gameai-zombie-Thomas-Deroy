#include "ExploreState.h"
#include "../SurvivorFSM.h"
#include "Common/InventoryComponent.h"
#include "Survivor/SurvivorPawn.h"
#include "Village/House/House.h"

void UExploreState::Enter(USurvivorFSM* FSM)
{
	Super::Enter(FSM);
    
    ParanoiaTimer = 0.0f;
    TimeUntilNextCheck = FMath::RandRange(8.0f, 15.0f);
    bIsParanoiaChecking = false;
    
    if (ContextFSM && ContextFSM->SurvivorPawn)
    {
        ContextFSM->SurvivorPawn->StopRunning();
    }
    
	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, TEXT("State Changed To: Explore"));
}

void UExploreState::Update(float DeltaTime)
{
    Super::Update(DeltaTime);
    
    if (!ContextFSM || !ContextFSM->SurvivorPawn) return;
    
    // Spinning
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
        
        return; 
    }
    
    // Check if target house was reached
    if (ContextFSM->TargetHouse)
    {
        // Distance to destination
        float DistToDestination = FLT_MAX;

        if (ContextFSM->CurrentPath.Num() > 0)
        {
            DistToDestination = FVector::Distance(ContextFSM->SurvivorPawn->GetActorLocation(), ContextFSM->CurrentPath.Last());
        }
        
        // Generate search point when we are there
        if (DistToDestination < 100.0f || ContextFSM->CurrentPath.IsEmpty() || ContextFSM->CurrentPathIndex >= ContextFSM->CurrentPath.Num())
        {
            if (SearchWaypoints > 0)
            {
                // Continue searching
                SearchWaypoints--;
                FHouseBounds Bounds = ContextFSM->TargetHouse->GetBounds();
                
                // Random point inside house bounds
                FVector RandomOffset;
                RandomOffset.X = FMath::RandRange(-Bounds.Extent.X * 0.4f, Bounds.Extent.X * 0.4f);
                RandomOffset.Y = FMath::RandRange(-Bounds.Extent.Y * 0.4f, Bounds.Extent.Y * 0.4f);
                RandomOffset.Z = 0.0f;

                FVector SearchPoint = Bounds.Origin + RandomOffset;
                ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(SearchPoint);
                ContextFSM->CurrentPathIndex = 0;
                return;
            }
            else
            {
                // Fully searched
                ContextFSM->VisitedHouses.Add(ContextFSM->TargetHouse);
                ContextFSM->KnownHouses.Remove(ContextFSM->TargetHouse);

                ContextFSM->TargetHouse = nullptr;
                ContextFSM->CurrentPath.Empty(); 
            }
        }
    }
    
    // Generate new path if needed
    if (ContextFSM->CurrentPath.IsEmpty() || ContextFSM->CurrentPathIndex >= ContextFSM->CurrentPath.Num())
    {
        UHealthComponent* HealthComp = ContextFSM->SurvivorPawn->GetComponentByClass<UHealthComponent>();
        float HealthPct = HealthComp ? (float)HealthComp->GetHealth() / (float)HealthComp->GetMaxHealth() : 1.0f;
        
        UStaminaComponent* StamComp = ContextFSM->SurvivorPawn->GetComponentByClass<UStaminaComponent>();
        float StaminaPct = StamComp ? StamComp->GetCurrentStamina() / StamComp->GetMaxStamina() : 1.0f;

        UInventoryComponent* Inventory = ContextFSM->SurvivorPawn->GetComponentByClass<UInventoryComponent>();
        bool bHasMedkit = false;
        bool bHasFood = false;

        for (auto const& Item : Inventory->GetInventory()) {
            if (!Item) continue;
            if (Item->GetItemType() == EItemType::Medkit) bHasMedkit = true;
            if (Item->GetItemType() == EItemType::Food) bHasFood = true;
        }

        // Agent remembers where the medkit is, if he needs it
        if (HealthPct < 0.5f && !bHasMedkit)
        {
            if (AActor* RememberedMedkit = ContextFSM->GetClosestKnownItem(EItemType::Medkit))
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, TEXT("Remembered a Medkit! Heading there!"));
                ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(RememberedMedkit->GetActorLocation());
                ContextFSM->CurrentPathIndex = 0;
                if (!ContextFSM->CurrentPath.IsEmpty()) return; 
            }
        }
        
        // Agent remembers where food is, if he needs it
        else if (StaminaPct < 0.4f && !bHasFood)
        {
            if (AActor* RememberedFood = ContextFSM->GetClosestKnownItem(EItemType::Food))
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Orange, TEXT("Remembered Food! Heading there!"));
                ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(RememberedFood->GetActorLocation());
                ContextFSM->CurrentPathIndex = 0;
                if (!ContextFSM->CurrentPath.IsEmpty()) return; 
            }
        }
        
        // Resume path to target house, if movement interrupted
        if (ContextFSM->TargetHouse)
        {
            ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(ContextFSM->TargetHouse->GetBounds().Origin);
            ContextFSM->CurrentPathIndex = 0;

            return;
        }
        
        // Move to known house first
        if (ContextFSM->KnownHouses.Num() > 0 && !ContextFSM->TargetHouse)
        {
            ContextFSM->TargetHouse = ContextFSM->KnownHouses[0];
            SearchWaypoints = 2; 
            
            ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(ContextFSM->TargetHouse->GetBounds().Origin);
            ContextFSM->CurrentPathIndex = 0; 
            return;
        }
        
        // Wander in random direction
        FVector ForwardDir = ContextFSM->SurvivorPawn->GetActorForwardVector();

        float RandomAngle = FMath::RandRange(-60.0f, 60.0f);

        FVector RandomDirection = ForwardDir.RotateAngleAxis(RandomAngle, FVector::UpVector);
        RandomDirection.Z = 0.0f; 
        RandomDirection.Normalize();

        FVector TargetLocation = ContextFSM->SurvivorPawn->GetActorLocation() + (RandomDirection * 1500.0f);

        ContextFSM->CurrentPath = ContextFSM->SurvivorPawn->CalculatePath(TargetLocation);
        ContextFSM->CurrentPathIndex = 0; 
    }
    else
    {
        // Follow current path
        ContextFSM->MoveAlongPath(DeltaTime);
    }
    
    if (bIsParanoiaChecking)
    {
        if (ParanoiaCheckDuration == 0.0f) 
        {
            CurrentParanoiaRot = ContextFSM->SurvivorPawn->GetActorRotation();
        }

        ParanoiaCheckDuration += DeltaTime;
        
        FVector MoveDir = ContextFSM->SurvivorPawn->GetVelocity().GetSafeNormal();
        if (MoveDir.IsNearlyZero()) MoveDir = ContextFSM->SurvivorPawn->GetActorForwardVector();

        FRotator LookBackRot = MoveDir.Rotation();
        LookBackRot.Yaw += ParanoiaAngle; 
        LookBackRot.Normalize();
        
        CurrentParanoiaRot = FMath::RInterpTo(CurrentParanoiaRot, LookBackRot, DeltaTime, 15.0f);
        ContextFSM->SurvivorPawn->SetActorRotation(CurrentParanoiaRot);
        
        // 1,5 second looking
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
        // Count down
        ParanoiaTimer += DeltaTime;
        if (ParanoiaTimer >= TimeUntilNextCheck)
        {
            bIsParanoiaChecking = true;
            ParanoiaCheckDuration = 0.0f;
            ParanoiaAngle = FMath::RandRange(160.0f, 200.0f); 
        }
    }
}

void UExploreState::Exit()
{
	Super::Exit();

	if (ContextFSM)
	{
		ContextFSM->CurrentPath.Empty(); 
	}
}