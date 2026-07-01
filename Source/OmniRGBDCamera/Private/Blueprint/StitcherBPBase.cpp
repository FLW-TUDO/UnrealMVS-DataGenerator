#include "Blueprint/StitcherBPBase.h"

void UStitcherBPBase::CancelStitching()
{
	if (bIsStitching)
	{
		bCancelRequested = true;
		UE_LOG(LogTemp, Display, TEXT("%s Cancellation requested"), *GetLogPrefix());
	}
}

void UStitcherBPBase::StartProgressPolling()
{
	ProgressTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([this](float DeltaTime) -> bool
		{
			if (bIsStitching)
			{
				int32 Current, Total;
				if (GetStitcherProgress(Current, Total) && Total > 0)
				{
					BroadcastProgress(Current, Total);
				}
			}
			return true; // Keep ticking
		}),
		0.1f  // Every 100ms
	);
}

void UStitcherBPBase::StopProgressPolling()
{
	if (ProgressTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ProgressTickerHandle);
		ProgressTickerHandle.Reset();
	}
}

void UStitcherBPBase::BroadcastProgress(int32 Current, int32 Total)
{
	OnStitchingProgress.Broadcast(Current, Total);
}
