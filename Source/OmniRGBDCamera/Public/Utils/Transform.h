#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Transform.generated.h"

UCLASS()
class OMNIRGBDCAMERA_API UTransformUtils : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Utils|Transform",
          meta=(DisplayName="Make Circular Camera Transforms"))
    static void MakeCircularCameraTransforms(
        int32 NumCameras,
        float StartOffsetDeg,
        float Radius,
        float AngleNoiseDeg,
        float RadiusNoise,
        TArray<FTransform>& OutTransforms
    );
};

