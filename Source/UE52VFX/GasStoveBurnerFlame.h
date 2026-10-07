// Copyright Epic Games, Inc. All Rights Reserved.
// Gas burner blue flame ring — material-based VFX, works with or without Niagara.
// Matches photo: ~24 small blue tongues on a circle, white-hot roots, blue transparent tips.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GasStoveBurnerFlame.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UNiagaraComponent;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class USoundBase;
class UAudioComponent;

/** Local axis the knob mesh spins around. */
UENUM(BlueprintType)
enum class EKnobAxis : uint8
{
	X,
	Y,
	Z
};

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "Gas Stove Burner Flame"))
class UE52VFX_API AGasStoveBurnerFlame : public AActor
{
	GENERATED_BODY()

public:
	AGasStoveBurnerFlame();

	/** Radius of the burner ring in cm (photo: tongues sit on a ~8-10cm circle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (ClampMin = "2", ClampMax = "40"))
	float BurnerRadius = 3.0f;

	/** Number of flame tongues around the ring. Photo shows ~20-24. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (ClampMin = "8", ClampMax = "64"))
	int32 FlameCount = 20;

	/** Height of one tongue in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (ClampMin = "2", ClampMax = "30"))
	float FlameHeight = 6.0f;

	/** Width of one tongue in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (ClampMin = "1", ClampMax = "12"))
	float FlameWidth = 1.3f;

	/** Outward lean of tongues in degrees (photo: flames lean slightly outward). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame")
	float OutwardTiltDeg = 33.0f;

	/** Emissive boost passed to M_GasFlame parameter FlameIntensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (ClampMin = "0", ClampMax = "60"))
	float FlameIntensity = 0.35f;

	/** Flicker speed (Hz-ish scale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame")
	float FlickerSpeed = 2.0f;

	/** Relative height flicker 0..0.6. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (ClampMin = "0", ClampMax = "0.6"))
	float FlickerAmount = 0.1f;

	/** Sideways dance amplitude in cm (per-tongue phase, base stays glued). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (ClampMin = "0", ClampMax = "5"))
	float SwayAmplitude = 0.18f;

	/** Sideways dance speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame")
	float SwaySpeed = 30.0f;

	/** Tilt oscillation in degrees around OutwardTiltDeg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (ClampMin = "0", ClampMax = "15"))
	float TiltWobbleDeg = 4.0f;

	/** Base point-light brightness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame")
	float LightIntensity = 0.6f;

	/** Turn flame on/off (for gameplay: gas valve). Off by default: place, keep hidden, ignite via SetLit/SetGasLevel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame")
	bool bLit = false;

	/** Gas knob 0..1: scales flame height, brightness and light live. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (ClampMin = "0", ClampMax = "1"))
	float GasLevel = 0.25f;

	UFUNCTION(BlueprintPure, Category = "Gas Flame")
	bool IsLit() const { return bLit; }

	/** E-interaction hint texts (project interaction system). Empty = engine defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame|Interaction")
	FText ActionTextOn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame|Interaction")
	FText ActionTextOff;

	/** Burning loop volume (auto-loaded from /Game/VFX/GasStove/Audio if empty). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame|Audio", meta = (ClampMin = "0", ClampMax = "2"))
	float CombustionVolume = 0.75f;

	/** Ignition one-shot volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame|Audio", meta = (ClampMin = "0", ClampMax = "2"))
	float IgnitionVolume = 0.3f;

	/** Valve-off click volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame|Audio", meta = (ClampMin = "0", ClampMax = "2"))
	float OffVolume = 1.0f;

	/** Flame appears this many seconds after the ignition click (syncs with the whoosh). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame|Audio", meta = (ClampMin = "0", ClampMax = "5"))
	float IgnitionDelay = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame|Audio")
	TObjectPtr<USoundBase> IgnitionSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame|Audio")
	TObjectPtr<USoundBase> CombustionSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame|Audio")
	TObjectPtr<USoundBase> OffSound;

	const FText& GetActionTextOn() const { return ActionTextOn; }
	const FText& GetActionTextOff() const { return ActionTextOff; }

	UFUNCTION(BlueprintCallable, Category = "Gas Flame")
	void SetLit(bool bNewLit);

	/** Flip the valve: off -> on, on -> off. */
	UFUNCTION(BlueprintCallable, Category = "Gas Flame")
	void ToggleLit();

	/** Knob mesh component (e.g. button mesh inside the stove BP) that controls this burner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame")
	TObjectPtr<UPrimitiveComponent> KnobMesh;

	/** Clicking KnobMesh in game toggles the flame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (EditCondition = "KnobMesh"))
	bool bKnobClickToggles = true;

	/** How far the knob turns when lit (like a real stove). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (EditCondition = "ControlKnob", ClampMin = "0", ClampMax = "180"))
	float KnobTurnDeg = 60.0f;

	/** Knob turn animation speed (degrees per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (EditCondition = "ControlKnob", ClampMin = "1"))
	float KnobTurnSpeed = 240.0f;

	/** Local axis of the knob mesh it spins around. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (EditCondition = "ControlKnob"))
	EKnobAxis KnobAxis = EKnobAxis::Z;

	/** Gas valve 0 (closed) .. 1 (full). Auto-hides flame near zero. */
	UFUNCTION(BlueprintCallable, Category = "Gas Flame")
	void SetGasLevel(float Level01);

	/** Rebuild the ring after changing FlameCount/BurnerRadius at runtime. */
	UFUNCTION(BlueprintCallable, Category = "Gas Flame")
	void RebuildFlames();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	void RebuildRing();
	void ApplyLitState();
	void BindKnob();
	void ComputeKnobAxis();
	void OnIgniteTimer();
	void ApplySpatialization(UAudioComponent* Audio);

	UFUNCTION()
	void OnKnobClicked(UPrimitiveComponent* TouchedComponent, FKey Button);

	UPROPERTY(VisibleAnywhere, Category = "Gas Flame")
	TObjectPtr<USceneComponent> Root;

	/** Crossed quads: 2 planes per tongue so flame is visible from all sides. */
	UPROPERTY(VisibleAnywhere, Category = "Gas Flame")
	TObjectPtr<UInstancedStaticMeshComponent> FlameInstances;

	/** Soft blue glow disc at burner base (uses T_GasFlame_Glow via material param switch if needed). */
	UPROPERTY(VisibleAnywhere, Category = "Gas Flame")
	TObjectPtr<UStaticMeshComponent> BaseGlow;

	UPROPERTY(VisibleAnywhere, Category = "Gas Flame")
	TObjectPtr<UPointLightComponent> BurnerLight;

	UPROPERTY(VisibleAnywhere, Category = "Gas Flame")
	TObjectPtr<UNiagaraComponent> ExtraFX;

	/** Looping combustion audio, plays while lit. */
	UPROPERTY(VisibleAnywhere, Category = "Gas Flame")
	TObjectPtr<UAudioComponent> BurnerAudio;

	/** Ignition / valve-off one-shots (spatial, at the burner). */
	UPROPERTY(VisibleAnywhere, Category = "Gas Flame")
	TObjectPtr<UAudioComponent> ClickAudio;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FlameMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlowMID;

	/** Knob runtime state (rotation turn-back animates even when flame is off). */
	TWeakObjectPtr<UPrimitiveComponent> KnobPrim;
	FQuat KnobBaseQuat = FQuat::Identity;
	float KnobCurAngle = 0.0f;
	float KnobTargetAngle = 0.0f;
	/** Local shaft axis of the knob (auto-detected from the knob's world orientation). */
	FVector KnobAxisLocal = FVector::YAxisVector;

	/** Delayed-ignition state: flame visuals appear only after IgnitionDelay. */
	bool bFlameVisual = false;
	FTimerHandle IgniteTimerHandle;

	float RunningTime = 0.0f;
	bool bRingBuilt = false;
};
