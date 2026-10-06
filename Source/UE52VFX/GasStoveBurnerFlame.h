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

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "Gas Stove Burner Flame"))
class UE52VFX_API AGasStoveBurnerFlame : public AActor
{
	GENERATED_BODY()

public:
	AGasStoveBurnerFlame();

	/** Radius of the burner ring in cm (photo: tongues sit on a ~8-10cm circle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (ClampMin = "2", ClampMax = "40"))
	float BurnerRadius = 9.0f;

	/** Number of flame tongues around the ring. Photo shows ~20-24. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (ClampMin = "8", ClampMax = "64"))
	int32 FlameCount = 22;

	/** Height of one tongue in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (ClampMin = "2", ClampMax = "30"))
	float FlameHeight = 8.0f;

	/** Width of one tongue in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (ClampMin = "1", ClampMax = "12"))
	float FlameWidth = 5.0f;

	/** Outward lean of tongues in degrees (photo: flames lean slightly outward). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame")
	float OutwardTiltDeg = 18.0f;

	/** Emissive boost passed to M_GasFlame parameter FlameIntensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (ClampMin = "0", ClampMax = "60"))
	float FlameIntensity = 6.0f;

	/** Flicker speed (Hz-ish scale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame")
	float FlickerSpeed = 13.0f;

	/** Relative height flicker 0..0.6. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (ClampMin = "0", ClampMax = "0.6"))
	float FlickerAmount = 0.30f;

	/** Sideways dance amplitude in cm (per-tongue phase, base stays glued). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (ClampMin = "0", ClampMax = "5"))
	float SwayAmplitude = 0.8f;

	/** Sideways dance speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame")
	float SwaySpeed = 9.0f;

	/** Tilt oscillation in degrees around OutwardTiltDeg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (ClampMin = "0", ClampMax = "15"))
	float TiltWobbleDeg = 4.0f;

	/** Base point-light brightness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame")
	float LightIntensity = 60.0f;

	/** Turn flame on/off (for gameplay: gas valve). Off by default: place, keep hidden, ignite via SetLit/SetGasLevel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame")
	bool bLit = false;

	/** Gas knob 0..1: scales flame height, brightness and light live. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (ClampMin = "0", ClampMax = "1"))
	float GasLevel = 1.0f;

	UFUNCTION(BlueprintCallable, Category = "Gas Flame")
	void SetLit(bool bNewLit);

	/** Flip the valve: off -> on, on -> off. */
	UFUNCTION(BlueprintCallable, Category = "Gas Flame")
	void ToggleLit();

	/** Knob actor (e.g. the stove's PlaneCutOtherPart_* mesh) that controls this burner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame")
	TObjectPtr<AActor> ControlKnob;

	/** Clicking ControlKnob in game toggles the flame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Flame", meta = (EditCondition = "ControlKnob"))
	bool bKnobClickToggles = true;

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

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FlameMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlowMID;

	float RunningTime = 0.0f;
	bool bRingBuilt = false;
};
