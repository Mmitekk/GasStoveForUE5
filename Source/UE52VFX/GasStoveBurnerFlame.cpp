// Copyright Epic Games, Inc. All Rights Reserved.

#include "GasStoveBurnerFlame.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
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
	if (!bLit)
	{
		SetActorTickEnabled(false);
	}
	RebuildRing();
	ApplyLitState();
	BindKnob();
}

void AGasStoveBurnerFlame::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ControlKnob)
	{
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(ControlKnob->GetRootComponent()))
		{
			Prim->OnClicked.RemoveAll(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AGasStoveBurnerFlame::BindKnob()
{
	if (!ControlKnob || !bKnobClickToggles)
	{
		return;
	}
	if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(ControlKnob->GetRootComponent()))
	{
		// StaticMeshActors are clickable by default, but make sure.
		Prim->SetClickable(true);
		Prim->OnClicked.AddDynamic(this, &AGasStoveBurnerFlame::OnKnobClicked);
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
	bLit = bNewLit;
	// Unlit burner costs nothing: no tick, hidden components, no draw calls.
	SetActorTickEnabled(bLit);
	ApplyLitState();
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
	const bool bShow = bLit;
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
	if (!bLit || !bRingBuilt)
	{
		return;
	}
	RunningTime += DeltaSeconds;

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
