#include "Utils/Transform.h"

DEFINE_LOG_CATEGORY_STATIC(LogTransformUtils, Log, All);

void UTransformUtils::MakeCircularCameraTransforms(
    int32 NumCameras,
    float StartOffsetDeg,
    float Radius,
    float AngleNoiseDeg,
    float RadiusNoise,
    TArray<FTransform>& OutTransforms)
{
    OutTransforms.Reset();

    if (NumCameras <= 0 || Radius <= 0.0f)
    {
        UE_LOG(LogTransformUtils, Warning,
            TEXT("Invalid inputs -> early return (NumCameras <= 0 or Radius <= 0)."));
        return;
    }

    const float StepDeg = 360.0f / static_cast<float>(NumCameras);
    
    OutTransforms.Reserve(NumCameras);
    
    for (int32 i = 0; i < NumCameras; ++i)
    {
        const float AngleJitterDeg = (AngleNoiseDeg > 0.0f) ? FMath::FRandRange(-AngleNoiseDeg, AngleNoiseDeg) : 0.0f;
        const float RadiusJitter = (RadiusNoise > 0.0f) ? FMath::FRandRange(-RadiusNoise, RadiusNoise) : 0.0f;

        const float AngleDeg = StartOffsetDeg + (static_cast<float>(i) * StepDeg) + AngleJitterDeg;
        const float AngleRad = FMath::DegreesToRadians(AngleDeg);

        const float FinalRadius = FMath::Max(0.0f, Radius + RadiusJitter);

        const FVector RadialDirection(
            FMath::Cos(AngleRad),
            FMath::Sin(AngleRad),
            0.0f
        );

        const FVector CameraLocation = RadialDirection * FinalRadius;
        const FRotator CameraRotation = RadialDirection.Rotation();

        OutTransforms.Add(FTransform(CameraRotation, CameraLocation, FVector::OneVector));
    }
}