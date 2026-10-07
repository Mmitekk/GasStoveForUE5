// Copyright Epic Games, Inc. All Rights Reserved.

#include "GasStoveBurnerFlame.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_EDITOR
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#else
#include "NiagaraComponent.h"
#endif

static UStaticMesh* GetPlaneMesh()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane"));
	return PlaneFinder.Succeeded() ? PlaneFinder.Object : nullptr;
}

AGasStoveBurnerFlame::AGasStoveBurnerFlame()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	FlameInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FlameInstances"));
	FlameInstances->SetupAttachment(Root);
	FlameInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlameInstances->SetCastShadow(false);
	FlameInstances->bAffectDynamicIndirectLighting = false;
	if (UStaticMesh* Plane = GetPlaneMesh())
	{
		FlameInstances->SetStaticMesh(Plane);
	}

	BaseGlow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseGlow"));
	BaseGlow->SetupAttachment(Root);
	BaseGlow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BaseGlow->SetCastShadow(false);
	if (UStaticMesh* Plane = GetPlaneMesh())
	{
		BaseGlow->SetStaticMesh(Plane);
	}
	BaseGlow->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	BaseGlow->SetRelativeLocation(FVector(0.0f, 0.0f, 0.6f));

	BurnerLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BurnerLight"));
	BurnerLight->SetupAttachment(Root);
	BurnerLight->SetRelativeLocation(FVector(0.0f, 0.0f, 8.0f));
	BurnerLight->LightColor = FColor(90, 150, 255);
	BurnerLight->Intensity = LightIntensity;
	BurnerLight->AttenuationRadius = 120.0f;
	BurnerLight->bUseInverseSquaredFalloff = false;
	BurnerLight->SetCastShadows(false);

	ExtraFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ExtraFX"));
	ExtraFX->SetupAttachment(Root);
	ExtraFX->bAutoActivate = true;

	BurnerAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("BurnerAudio"));
	BurnerAudio->SetupAttachment(Root);
	BurnerAudio->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));
	BurnerAudio->bAutoActivate = false;
}

void AGasStoveBurnerFlame::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// NOTE: LoadObject, NOT FObjectFinder — finders are illegal outside constructors (fatal error).
	// No member caching of base materials: stale hard refs block asset replace/delete.
	UMaterialInterface* FlameBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/GasStove/M_GasFlame"));
	if (FlameBase)
	{
		FlameMID = UMaterialInstanceDynamic::Create(FlameBase, this);
		FlameInstances->SetMaterial(0, FlameMID);
		FlameMID->SetScalarParameterValue(TEXT("FlameIntensity"), FlameIntensity);
	}
	// Base glow: own radial material if present, else reuse flame MID.
	UMaterialInterface* GlowBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/GasStove/M_GasGlow"));
	if (GlowBase)
	{
		GlowMID = UMaterialInstanceDynamic::Create(GlowBase, this);
		BaseGlow->SetMaterial(0, GlowMID);
	}
	else if (FlameMID)
	{
		BaseGlow->SetMaterial(0, FlameMID);
	}

	// Sounds: auto-load the pack defaults if not overridden per-instance.
	if (!IgnitionSound)
	{
		IgnitionSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/VFX/GasStove/Audio/S_GasIgnition"));
	}
	if (!CombustionSound)
	{
		CombustionSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/VFX/GasStove/Audio/S_GasCombustion"));
	}
	if (!OffSound)
	{
		OffSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/VFX/GasStove/Audio/S_GasOff"));
	}

	RebuildRing();
	ApplyLitState();

	UE_LOG(LogTemp, Log, TEXT("[GasFlame] OnConstruction: instances=%d mesh=%s mat=%s"),
		FlameInstances ? FlameInstances->GetInstanceCount() : -1,
		(FlameInstances && FlameInstances->GetStaticMesh()) ? TEXT("ok") : TEXT("NULL"),
		FlameMID ? TEXT("ok") : TEXT("NULL"));
}

void AGasStoveBurnerFlame::BeginPlay()
{
	Super::BeginPlay();

	// Auto-bind knob by naming convention (zero setup): this flame's child actor
	// component "GasStoveBurnerFlame_vN" -> component "SM_FrameCabinStove_Button_vN"
	// on the parent stove BP. Manual KnobMesh picker wins if set.
	if (!KnobMesh)
	{
		AActor* Parent = GetAttachParentActor();
		UChildActorComponent* MyCAC = nullptr;
		if (Parent)
		{
			TInlineComponentArray<UChildActorComponent*> CACs(Parent);
			for (UChildActorComponent* C : CACs)
			{
				if (C && C->GetChildActor() == this)
				{
					MyCAC = C;
					break;
				}
			}
		}
		if (MyCAC && Parent)
		{
			const FString CacName = MyCAC->GetName();
			const int32 SufIdx = CacName.Find(TEXT("_v"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (SufIdx != INDEX_NONE)
			{
				const FString KnobName = TEXT("SM_FrameCabinStove_Button") + CacName.Mid(SufIdx);
				TInlineComponentArray<UPrimitiveComponent*> Prims(Parent);
				for (UPrimitiveComponent* P : Prims)
				{
					if (P && P->GetName().Equals(KnobName, ESearchCase::IgnoreCase))
					{
						KnobMesh = P;
						UE_LOG(LogTemp, Log, TEXT("[GasFlame] auto-bound knob '%s' (from %s)"),
							*KnobName, *CacName);
						break;
					}
				}
				if (!KnobMesh)
				{
					UE_LOG(LogTemp, Warning, TEXT("[GasFlame] knob '%s' NOT found on parent for %s"),
						*KnobName, *CacName);
				}
			}
		}
	}

	if (!bLit)
	{
		SetActorTickEnabled(false);
	}
	// Pre-lit burners (bLit saved true) show visuals immediately.
	bFlameVisual = bLit;
	RebuildRing();
	ApplyLitState();
	BindKnob();

	// A burner placed pre-lit starts its burn loop too.
	if (bLit && CombustionSound && BurnerAudio)
	{
		BurnerAudio->SetSound(CombustionSound);
		BurnerAudio->SetVolumeMultiplier(CombustionVolume);
		BurnerAudio->Play();
	}

#if __has_include("Interface/InteractionInterface.h")
	// Project (WinterHut) E-interaction: hook the knob via StoveKnobInteractive.
	// Class looked up by path so this file stays plugin-free and compiles anywhere.
	// Deferred spawn + attach BEFORE FinishSpawning so the interactor's BeginPlay
	// already sees its parent knob component (otherwise it self-destroys).
	if (KnobMesh)
	{
		UClass* InteractorClass = FindObject<UClass>(nullptr, TEXT("/Script/WinterHut.StoveKnobInteractive"));
		UE_LOG(LogTemp, Log, TEXT("[GasFlame] interactor class=%s"),
			InteractorClass ? *InteractorClass->GetName() : TEXT("NOT FOUND"));
		if (InteractorClass)
		{
			FActorSpawnParameters SP;
			SP.Owner = this;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* Interactor = GetWorld()->SpawnActorDeferred<AActor>(
				InteractorClass, KnobMesh->GetComponentTransform(), this,
				nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Interactor)
			{
				Interactor->AttachToComponent(KnobMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				Interactor->FinishSpawning(KnobMesh->GetComponentTransform());
				UE_LOG(LogTemp, Log, TEXT("[GasFlame] interactor spawned on knob %s"),
					*KnobMesh->GetName());
			}
		}
	}
#endif
}

void AGasStoveBurnerFlame::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (KnobMesh)
	{
		KnobMesh->OnClicked.RemoveAll(this);
	}
	if (BurnerAudio)
	{
		BurnerAudio->Stop();
	}
	Super::EndPlay(EndPlayReason);
}

void AGasStoveBurnerFlame::BindKnob()
{
	if (!KnobMesh || !bKnobClickToggles)
	{
		return;
	}
	// Click routing: PlayerController (bEnableClickEvents) -> OnClicked.
	KnobMesh->OnClicked.AddDynamic(this, &AGasStoveBurnerFlame::OnKnobClicked);
	// Remember rest pose so the knob can turn and return.
	KnobPrim = KnobMesh;
	KnobBaseQuat = KnobMesh->GetRelativeRotation().Quaternion();
	ComputeKnobAxis();
}

void AGasStoveBurnerFlame::ComputeKnobAxis()
{
	// The knob must TWIST around its own shaft. The shaft is the knob mesh's
	// THINNEST dimension (a mushroom button is thin along its shaft) — world-aligned
	// AABB tells which world direction that is; then map it to the local axis.
	const FBoxSphereBounds& B = KnobMesh->Bounds;
	FVector ThinWorld(1.0f, 0.0f, 0.0f);
	float MinE = B.BoxExtent.X;
	if (B.BoxExtent.Y < MinE)
	{
		MinE = B.BoxExtent.Y;
		ThinWorld = FVector(0.0f, 1.0f, 0.0f);
	}
	if (B.BoxExtent.Z < MinE)
	{
		ThinWorld = FVector(0.0f, 0.0f, 1.0f);
	}

	// Map world-thin direction to the closest local axis.
	float BestDot = -2.0f;
	FVector BestAxis = FVector::YAxisVector;
	const FVector Axes[3] = {
		KnobMesh->GetComponentTransform().GetUnitAxis(EAxis::X),
		KnobMesh->GetComponentTransform().GetUnitAxis(EAxis::Y),
		KnobMesh->GetComponentTransform().GetUnitAxis(EAxis::Z) };
	for (const FVector& A : Axes)
	{
		const float D = FMath::Abs(FVector::DotProduct(A, ThinWorld));
		if (D > BestDot)
		{
			BestDot = D;
			BestAxis = A;
		}
	}
	KnobAxisLocal = BestAxis;
}

void AGasStoveBurnerFlame::OnIgniteTimer()
{
	if (!bLit)
	{
		return; // valve closed before ignition finished
	}
	bFlameVisual = true;
	ApplyLitState();
	if (BurnerAudio && CombustionSound)
	{
		BurnerAudio->SetSound(CombustionSound);
		BurnerAudio->FadeIn(0.4f, CombustionVolume);
	}
}

void AGasStoveBurnerFlame::OnKnobClicked(UPrimitiveComponent* TouchedComponent, FKey Button)
{
	ToggleLit();
}

void AGasStoveBurnerFlame::ToggleLit()
{
	SetLit(!bLit);
}

void AGasStoveBurnerFlame::SetLit(bool bNewLit)
{
	const bool bWasLit = bLit;
	bLit = bNewLit;
	// Knob mirrors the valve: turn to KnobTurnDeg when lit, back to 0 when off.
	KnobTargetAngle = bLit ? KnobTurnDeg : 0.0f;
	// Tick drives flame flicker AND the knob turn-back animation.
	SetActorTickEnabled(bLit || !FMath::IsNearlyEqual(KnobCurAngle, KnobTargetAngle, 0.01f));

	// Flame visuals come with a delay to match the ignition whoosh.
	if (bLit && !bWasLit)
	{
		bFlameVisual = false;
		ApplyLitState();
		GetWorldTimerManager().SetTimer(IgniteTimerHandle, this,
			&AGasStoveBurnerFlame::OnIgniteTimer, FMath::Max(0.01f, IgnitionDelay), false);
		if (IgnitionSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, IgnitionSound, GetActorLocation(), IgnitionVolume);
		}
	}
	else if (!bLit && bWasLit)
	{
		GetWorldTimerManager().ClearTimer(IgniteTimerHandle);
		bFlameVisual = false;
		ApplyLitState();
		if (OffSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, OffSound, GetActorLocation(), OffVolume);
		}
		if (BurnerAudio)
		{
			BurnerAudio->FadeOut(0.3f, 0.0f);
		}
	}
}

void AGasStoveBurnerFlame::SetGasLevel(float Level01)
{
	GasLevel = FMath::Clamp(Level01, 0.0f, 1.0f);
	SetLit(GasLevel > 0.02f);
}

void AGasStoveBurnerFlame::RebuildFlames()
{
	RebuildRing();
}

void AGasStoveBurnerFlame::ApplyLitState()
{
	// Editor preview follows bLit directly; in game visuals respect ignition delay.
	const bool bShow = GetWorld() && GetWorld()->IsGameWorld() ? bFlameVisual : bLit;
	FlameInstances->SetVisibility(bShow);
	BaseGlow->SetVisibility(bShow);
	BurnerLight->SetVisibility(bShow);
	if (ExtraFX && ExtraFX->GetAsset())
	{
		if (bShow) ExtraFX->Activate(true);
		else ExtraFX->Deactivate();
	}
}

void AGasStoveBurnerFlame::RebuildRing()
{
	if (!FlameInstances || !FlameInstances->GetStaticMesh())
	{
		return;
	}
	FlameInstances->ClearInstances();

	const int32 Count = FMath::Clamp(FlameCount, 1, 64);
	// Engine Plane is 100x100 cm in XY, normal +Z (lies flat).
	// Petal orientation is built from an explicit orthonormal basis
	// (rotators can't express "stand up + lean outward" unambiguously):
	//   local X = width dir (tangent for fin 0, radial for fin 1),
	//   local Y = height dir (up leaned outward by OutwardTiltDeg).
	const float ScaleX = FlameWidth / 100.0f;
	const float ScaleY = FlameHeight / 100.0f;
	const float TiltRad = FMath::DegreesToRadians(OutwardTiltDeg);
	const FVector WorldUp(0.0f, 0.0f, 1.0f);
	// Gas knob preview: works in Details without Play (Tick keeps it live in game).
	const float HPrev = FMath::Max(0.05f, GasLevel);

	for (int32 i = 0; i < Count; ++i)
	{
		const float AngRad = 2.0f * PI * static_cast<float>(i) / static_cast<float>(Count);
		const FVector Radial(FMath::Cos(AngRad), FMath::Sin(AngRad), 0.0f);
		const FVector Tangent(-Radial.Y, Radial.X, 0.0f);
		// Up vector leaned outward so petals splay like the photo.
		const FVector LeanUp = (WorldUp * FMath::Cos(TiltRad) + Radial * FMath::Sin(TiltRad)).GetSafeNormal();
		const FVector Base = Radial * BurnerRadius;

		// Fin 0 faces outward (full-face from outside),
		// fin 1 is perpendicular (cross) so petals read from all sides.
		for (int32 k = 0; k < 2; ++k)
		{
			const FVector WidthDir = (k == 0) ? Tangent : Radial;
			const FMatrix Basis(FRotationMatrix::MakeFromXY(WidthDir, LeanUp));
			FTransform T;
			T.SetLocation(Base + LeanUp * (FlameHeight * 0.5f * HPrev));
			T.SetRotation(FQuat(Basis));
			T.SetScale3D(FVector(ScaleX, ScaleY * HPrev, 1.0f));
			FlameInstances->AddInstance(T, false);
		}
	}

	// Base glow disc: slightly larger than burner ring.
	const float GlowR = (BurnerRadius * 2.0f + 6.0f) / 100.0f;
	BaseGlow->SetRelativeScale3D(FVector(GlowR, GlowR, 1.0f));

	// Gas knob preview for editor viewport (no Play needed).
	BurnerLight->SetIntensity(LightIntensity * GasLevel);
	if (FlameMID)
	{
		FlameMID->SetScalarParameterValue(TEXT("FlameIntensity"), FlameIntensity * (0.25f + 0.75f * GasLevel));
	}

	bRingBuilt = true;
}

void AGasStoveBurnerFlame::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RunningTime += DeltaSeconds;

	// Knob turn animation: runs both directions (also while flame is off,
	// so the knob visibly turns back after switching the burner off).
	if (KnobPrim.IsValid() && !FMath::IsNearlyEqual(KnobCurAngle, KnobTargetAngle, 0.01f))
	{
		const float Step = KnobTurnSpeed * DeltaSeconds;
		KnobCurAngle = (KnobTargetAngle > KnobCurAngle)
			? FMath::Min(KnobCurAngle + Step, KnobTargetAngle)
			: FMath::Max(KnobCurAngle - Step, KnobTargetAngle);
		const FVector Axis = (KnobAxis == EKnobAxis::X) ? FVector(1.0f, 0.0f, 0.0f)
			: (KnobAxis == EKnobAxis::Y) ? FVector(0.0f, 1.0f, 0.0f)
			: FVector(0.0f, 0.0f, 1.0f);
		KnobPrim->SetRelativeRotation(KnobBaseQuat * FQuat(Axis, FMath::DegreesToRadians(KnobCurAngle)));
	}

	if (!bFlameVisual || !bRingBuilt)
	{
		// Flame off and knob settled -> full sleep, zero cost.
		if (!bLit && FMath::IsNearlyEqual(KnobCurAngle, KnobTargetAngle, 0.01f))
		{
			SetActorTickEnabled(false);
		}
		return;
	}

	const int32 Total = FlameInstances->GetInstanceCount();
	const int32 Count = FMath::Clamp(FlameCount, 1, 64);
	const float ScaleX = FlameWidth / 100.0f;
	const float ScaleY = FlameHeight / 100.0f;
	const float TiltRad = FMath::DegreesToRadians(OutwardTiltDeg);

	// Per-tongue height flicker: two sines with per-instance phase.
	// Cheap enough for <=128 instances.
	for (int32 Idx = 0; Idx < Total; ++Idx)
	{
		FTransform T;
		if (!FlameInstances->GetInstanceTransform(Idx, T, false))
		{
			continue;
		}
		const int32 TongueIdx = Idx / 2;
		const float Phase = static_cast<float>(TongueIdx) * 2.39996f; // golden angle -> neighbours differ
		const float N1 = FMath::Sin(RunningTime * FlickerSpeed + Phase);
		const float N2 = FMath::Sin(RunningTime * FlickerSpeed * 2.33f + Phase * 1.71f);
		const float Flick = 1.0f + FlickerAmount * (0.65f * N1 + 0.35f * N2);
		// Gas knob: shrink height toward burner as valve closes (never fully zero
		// so the ring doesn't pop — SetGasLevel hides the actor at ~0).
		const float HScale = Flick * FMath::Max(0.05f, GasLevel);

		FVector S = T.GetScale3D();
		S.Y = ScaleY * HScale;
		// Slight width pinch when tall (volume-ish preservation).
		S.X = ScaleX * (1.0f - 0.18f * (Flick - 1.0f));
		S.Z = 1.0f;
		T.SetScale3D(S);

		// Keep base glued to burner: center = base + LeanUp * half current height.
		// Plus sideways dance (tangent) and tilt wobble so flames lick.
		const float AngRad = 2.0f * PI * static_cast<float>(TongueIdx % Count) / static_cast<float>(Count);
		const FVector Radial(FMath::Cos(AngRad), FMath::Sin(AngRad), 0.0f);
		const FVector Tangent(-Radial.Y, Radial.X, 0.0f);
		const float Wob = FMath::DegreesToRadians(TiltWobbleDeg)
			* FMath::Sin(RunningTime * (FlickerSpeed * 0.66f) + Phase * 1.31f);
		const float TiltNow = TiltRad + Wob;
		const FVector LeanUp = (FVector(0.0f, 0.0f, 1.0f) * FMath::Cos(TiltNow)
			+ Radial * FMath::Sin(TiltNow)).GetSafeNormal();
		const float Sway = SwayAmplitude * FMath::Sin(RunningTime * SwaySpeed + Phase * 1.7f);
		const FVector WidthDir = ((Idx % 2) == 0) ? Tangent : Radial;
		T.SetRotation(FQuat(FMatrix(FRotationMatrix::MakeFromXY(WidthDir, LeanUp))));
		T.SetLocation(Radial * BurnerRadius + LeanUp * (FlameHeight * 0.5f * HScale) + Tangent * Sway);

		FlameInstances->UpdateInstanceTransform(Idx, T, false, false, false);
	}
	// One transform flush per frame would need MarkRenderStateDirty; UpdateInstanceTransform
	// with bTeleport=false already dirties. Force a redraw of bounds:
	FlameInstances->MarkRenderStateDirty();

	// Light flicker: fast small + slow breathing, like gas.
	const float LFlick = 1.0f
		+ 0.10f * FMath::Sin(RunningTime * 23.0f)
		+ 0.06f * FMath::Sin(RunningTime * 47.0f + 1.3f);
	BurnerLight->SetIntensity(LightIntensity * GasLevel * FMath::Max(0.2f, LFlick));

	if (FlameMID)
	{
		FlameMID->SetScalarParameterValue(TEXT("FlameIntensity"), FlameIntensity * (0.92f + 0.08f * LFlick) * (0.25f + 0.75f * GasLevel));
		FlameMID->SetScalarParameterValue(TEXT("FlameTime"), RunningTime);
	}
}
