// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"

/**
 * 
 */
class GRID_API FSurfaceFrame
{
public:
	FVector Origin;  // 原点在世界坐标系中的位置
	FVector Up;      // 天方向
	FVector Right;   // 东方向
	FVector Forward; // 北方向
	FSurfaceFrame();
	~FSurfaceFrame();
};
