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

bool ASFogMask::TriangleRowSpan(const FVector2D& A, const FVector2D& B, const FVector2D& C, double Area,
	int32 Y, int32 MinX, int32 MaxX, int32& OutMinX, int32& OutMaxX)
{
	OutMinX = 0;
	OutMaxX = -1;
	if (MinX > MaxX || FMath::IsNearlyZero(Area))
	{
		return false;
	}

	auto Edge = [](const FVector2D& P0, const FVector2D& P1, const FVector2D& P)
	{
		return (P1.X - P0.X) * (P.Y - P0.Y) - (P1.Y - P0.Y) * (P.X - P0.X);
	};
	const double CenterY = Y + 0.5;
	const bool bPositiveArea = Area > 0.0;
	auto IsInside = [&](int32 X)
	{
		const FVector2D P(X + 0.5, CenterY);
		const double W0 = Edge(B, C, P);
		const double W1 = Edge(C, A, P);
		const double W2 = Edge(A, B, P);
		return bPositiveArea ? (W0 >= 0.0 && W1 >= 0.0 && W2 >= 0.0)
			: (W0 <= 0.0 && W1 <= 0.0 && W2 <= 0.0);
	};

	double LowerX = MinX;
	double UpperX = MaxX;
	const FVector2D Starts[] = { B, C, A };
	const FVector2D Ends[] = { C, A, B };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FVector2D& P0 = Starts[Index];
		const FVector2D& P1 = Ends[Index];
		double Slope = -(P1.Y - P0.Y);
		double Value = Edge(P0, P1, FVector2D(MinX + 0.5, CenterY));
		if (!bPositiveArea)
		{
			Slope = -Slope;
			Value = -Value;
		}
		if (Slope == 0.0)
		{
			if (Value < 0.0)
			{
				return false;
			}
			continue;
		}
		const double Bound = MinX - Value / Slope;
		if (Slope > 0.0)
		{
			LowerX = FMath::Max(LowerX, Bound);
		}
		else
		{
			UpperX = FMath::Min(UpperX, Bound);
		}
	}

	// 정수 변환 전에 경계로 제한하고 반올림 오차로 경계 셀을 놓치지 않도록 양쪽을 1셀 넓힌다.
	int32 StartX = FMath::CeilToInt(FMath::Clamp(LowerX, static_cast<double>(MinX), static_cast<double>(MaxX)));
	int32 EndX = FMath::FloorToInt(FMath::Clamp(UpperX, static_cast<double>(MinX), static_cast<double>(MaxX)));
	StartX = FMath::Max(MinX, StartX - 1);
	EndX = FMath::Min(MaxX, EndX + 1);
	while (StartX <= EndX && !IsInside(StartX))
	{
		++StartX;
	}
	while (EndX >= StartX && !IsInside(EndX))
	{
		--EndX;
	}
	if (StartX > EndX)
	{
		return false;
	}
	// 원래 판정으로 양 끝 바깥도 확인해 포함 셀을 놓치지 않는다.
	while (StartX > MinX && IsInside(StartX - 1))
	{
		--StartX;
	}
	while (EndX < MaxX && IsInside(EndX + 1))
	{
		++EndX;
	}
	OutMinX = StartX;
	OutMaxX = EndX;
	return true;
}

void ASFogMask::BlurAlphaRegion(const TArray<uint8>& Source, TArray<uint8>& Dest, int32 Width, int32 Height, int32 Radius, const FIntRect& Region, TArray<uint8>& Scratch)
{
	Radius = FMath::Clamp(Radius, 0, 8);
	if (Width <= 0 || Height <= 0 || static_cast<int64>(Width) * Height != Source.Num() || Source.Num() != Dest.Num())
	{
		return;
	}

	const FIntRect Clipped(FMath::Clamp(Region.Min.X, 0, Width), FMath::Clamp(Region.Min.Y, 0, Height),
		FMath::Clamp(Region.Max.X, 0, Width), FMath::Clamp(Region.Max.Y, 0, Height));
	if (Clipped.Width() <= 0 || Clipped.Height() <= 0)
	{
		return;
	}
	check(&Source != &Dest && &Source != &Scratch && &Dest != &Scratch);
	if (Radius == 0)
	{
		for (int32 Y = Clipped.Min.Y; Y < Clipped.Max.Y; ++Y)
		{
			const int32 Start = Y * Width + Clipped.Min.X;
			FMemory::Memcpy(Dest.GetData() + Start, Source.GetData() + Start, Clipped.Width());
		}
		return;
	}

	const int32 FirstRow = FMath::Max(0, Clipped.Min.Y - Radius);
	const int32 LastRow = FMath::Min(Height, Clipped.Max.Y + Radius);
	const int32 ScratchWidth = Clipped.Width();
	// 작은 변경 영역을 연속 처리해도 버퍼를 줄였다 다시 할당하지 않는다.
	Scratch.SetNumUninitialized(ScratchWidth * (LastRow - FirstRow), EAllowShrinking::No);

	// 파스칼 삼각형의 2R번째 행: 가중치 합은 정확히 2^(2R)이므로 상수 알파가 유지된다.
	int32 Weights[17];
	const int32 Order = Radius * 2;
	Weights[0] = 1;
	for (int32 Index = 1; Index <= Order; ++Index)
	{
		Weights[Index] = Weights[Index - 1] * (Order - Index + 1) / Index;
	}
	const int32 Divisor = 1 << Order;

	for (int32 Y = FirstRow; Y < LastRow; ++Y)
	{
		const int32 Row = Y * Width;
		for (int32 X = Clipped.Min.X; X < Clipped.Max.X; ++X)
		{
			int32 Sum = 0;
			for (int32 Offset = -Radius; Offset <= Radius; ++Offset)
			{
				const int32 SampleX = FMath::Clamp(X + Offset, 0, Width - 1);
				Sum += Source[Row + SampleX] * Weights[Offset + Radius];
			}
			Scratch[(Y - FirstRow) * ScratchWidth + X - Clipped.Min.X] = static_cast<uint8>((Sum + Divisor / 2) / Divisor);
		}
	}

	for (int32 Y = Clipped.Min.Y; Y < Clipped.Max.Y; ++Y)
	{
		for (int32 X = Clipped.Min.X; X < Clipped.Max.X; ++X)
		{
			int32 Sum = 0;
			for (int32 Offset = -Radius; Offset <= Radius; ++Offset)
			{
				const int32 SampleY = FMath::Clamp(Y + Offset, 0, Height - 1);
				Sum += Scratch[(SampleY - FirstRow) * ScratchWidth + X - Clipped.Min.X] * Weights[Offset + Radius];
			}
			Dest[Y * Width + X] = static_cast<uint8>((Sum + Divisor / 2) / Divisor);
		}
	}
}

bool ASFogMask::StepToward(TArray<uint8>& Shown, const TArray<uint8>& Target, int32 Width, int32 Height, int32 MaxStep, const FIntRect& Region, FIntRect& OutDirty, bool& bOutReachedTarget)
{
	OutDirty = FIntRect(0, 0, 0, 0);
	bOutReachedTarget = false;
	if (Width <= 0 || Height <= 0 || static_cast<int64>(Width) * Height != Shown.Num()
		|| Shown.Num() != Target.Num() || MaxStep <= 0)
	{
		return false;
	}

	const FIntRect Clipped(FMath::Clamp(Region.Min.X, 0, Width), FMath::Clamp(Region.Min.Y, 0, Height),
		FMath::Clamp(Region.Max.X, 0, Width), FMath::Clamp(Region.Max.Y, 0, Height));
	bOutReachedTarget = true;
	MaxStep = FMath::Min(MaxStep, 255);
	int32 MinX = Width;
	int32 MinY = Height;
	int32 MaxX = 0;
	int32 MaxY = 0;
	for (int32 Y = Clipped.Min.Y; Y < Clipped.Max.Y; ++Y)
	{
		for (int32 X = Clipped.Min.X; X < Clipped.Max.X; ++X)
		{
			const int32 Index = Y * Width + X;
			const int32 Delta = static_cast<int32>(Target[Index]) - Shown[Index];
			if (Delta == 0)
			{
				continue;
			}

			bOutReachedTarget &= FMath::Abs(Delta) <= MaxStep;
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
