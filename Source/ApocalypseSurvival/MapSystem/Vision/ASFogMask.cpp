#include "MapSystem/Vision/ASFogMask.h"

FIntPoint ASFogMask::ComputeResolution(FVector2D WorldSize, int32 ShortSide)
{
	if (!FMath::IsFinite(WorldSize.X) || !FMath::IsFinite(WorldSize.Y) || WorldSize.X <= 0.0 || WorldSize.Y <= 0.0)
	{
		return FIntPoint(16, 16);
	}

	const double ShortWorldSide = FMath::Min(WorldSize.X, WorldSize.Y);
	// 반올림 전에 범위를 제한해 큰 비율에서도 정수 오버플로를 피한다.
	return FIntPoint(
		FMath::RoundToInt(FMath::Clamp(static_cast<double>(ShortSide) * (WorldSize.Y / ShortWorldSide), 16.0, 2048.0)),
		FMath::RoundToInt(FMath::Clamp(static_cast<double>(ShortSide) * (WorldSize.X / ShortWorldSide), 16.0, 2048.0)));
}

void ASFogMask::BlurAlpha(TArray<uint8>& InOut, int32 Width, int32 Height, int32 Radius, TArray<uint8>& Scratch)
{
	Radius = FMath::Clamp(Radius, 0, 8);
	if (Radius == 0 || Width <= 0 || Height <= 0 || static_cast<int64>(Width) * Height != InOut.Num())
	{
		return;
	}

	check(&InOut != &Scratch);
	Scratch.SetNumUninitialized(InOut.Num());

	// 파스칼 삼각형의 2R번째 행: 가중치 합은 정확히 2^(2R)이므로 상수 알파가 유지된다.
	int32 Weights[17];
	const int32 Order = Radius * 2;
	Weights[0] = 1;
	for (int32 Index = 1; Index <= Order; ++Index)
	{
		Weights[Index] = Weights[Index - 1] * (Order - Index + 1) / Index;
	}
	const int32 Divisor = 1 << Order;

	for (int32 Y = 0; Y < Height; ++Y)
	{
		const int32 Row = Y * Width;
		for (int32 X = 0; X < Width; ++X)
		{
			int32 Sum = 0;
			for (int32 Offset = -Radius; Offset <= Radius; ++Offset)
			{
				const int32 SampleX = FMath::Clamp(X + Offset, 0, Width - 1);
				Sum += InOut[Row + SampleX] * Weights[Offset + Radius];
			}
			Scratch[Row + X] = static_cast<uint8>((Sum + Divisor / 2) / Divisor);
		}
	}

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			int32 Sum = 0;
			for (int32 Offset = -Radius; Offset <= Radius; ++Offset)
			{
				const int32 SampleY = FMath::Clamp(Y + Offset, 0, Height - 1);
				Sum += Scratch[SampleY * Width + X] * Weights[Offset + Radius];
			}
			InOut[Y * Width + X] = static_cast<uint8>((Sum + Divisor / 2) / Divisor);
		}
	}
}

bool ASFogMask::StepToward(TArray<uint8>& Shown, const TArray<uint8>& Target, int32 Width, int32 Height, int32 MaxStep, FIntRect& OutDirty)
{
	OutDirty = FIntRect(0, 0, 0, 0);
	if (Width <= 0 || Height <= 0 || static_cast<int64>(Width) * Height != Shown.Num()
		|| Shown.Num() != Target.Num() || MaxStep <= 0)
	{
		return false;
	}

	MaxStep = FMath::Min(MaxStep, 255);
	int32 MinX = Width;
	int32 MinY = Height;
	int32 MaxX = 0;
	int32 MaxY = 0;
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 Index = Y * Width + X;
			const int32 Delta = static_cast<int32>(Target[Index]) - Shown[Index];
			if (Delta == 0)
			{
				continue;
			}

			Shown[Index] = static_cast<uint8>(Shown[Index] + FMath::Clamp(Delta, -MaxStep, MaxStep));
			MinX = FMath::Min(MinX, X);
			MinY = FMath::Min(MinY, Y);
			MaxX = FMath::Max(MaxX, X + 1);
			MaxY = FMath::Max(MaxY, Y + 1);
		}
	}

	if (MinX == Width)
	{
		return false;
	}
	OutDirty = FIntRect(MinX, MinY, MaxX, MaxY);
	return true;
}
