// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Actor.h"
#include "Render/PlanetCameraController.h"
#include "Render/PlanetHISMTileRenderer.h"
#include "TerraGameplayContainer.h"
#include "TerraPiecePresentationTypes.h"
#include "Templates/UniquePtr.h"
#include "WorldGenSettings.h"   // T4：UPROPERTY 直接持有 FWorldGenSettings → 完整类型可见
#include "PlanetTessellatedMesh.generated.h"

class UTexture2D;
class UAnimationAsset;
class UAnimInstance;
class UAnimMontage;
class FSphereTopology;
class FWorldGenerator;   // T4：TUniquePtr<FWorldGenerator>，避免在头文件 include "WorldGenerator.h"
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class USkeletalMesh;
class UTerraPiecePresentationManager;
struct FHitResult;

enum class ETerraG1DebugPieceType : uint8
{
    Base,
    Infantry,
    Cavalry,
    Archer,
};

struct FTerraG1DebugPiece
{
    int32 FactionId = INDEX_NONE;
    int32 CellId = INDEX_NONE;
    ETerraG1DebugPieceType PieceType = ETerraG1DebugPieceType::Infantry;
};

static const FRotator EditorXYZRotator(double XRoll, double YPitch, double ZYaw)
{
    return FRotator(YPitch, ZYaw, XRoll);
}

/**
 * APlanetTessellatedMesh
 *
 * SimpleGameplay planet actor.
 * Owns gameplay/worldgen orchestration and delegates tile rendering/highlight
 * state to FPlanetHISMTileRenderer. The runtime sphere surface is assembled
 * from HISM tile instances; legacy ProceduralMesh/SDF surface paths are not
 * part of this actor anymore.
 */
UCLASS()
class TERRACIVILIZATION_API APlanetTessellatedMesh : public AActor
{
    GENERATED_BODY()
    friend class FPlanetCameraController;

public:
    APlanetTessellatedMesh();

    /** FVTableHelper constructor + destructor stay in .cpp because this class owns incomplete TUniquePtr types. */
    APlanetTessellatedMesh(FVTableHelper& Helper);
    virtual ~APlanetTessellatedMesh();

    //----------------------------------------------------------
    // §2.1 / §1.3 拍板参数（编辑器可调）
    //----------------------------------------------------------

    /** Cell 拓扑细分层级（玩法 / WorldGen 用）。默认 sub=3 → 642 cells。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess",
              meta = (ClampMin = "1", ClampMax = "5"))
    int32 CellSubdivisionLevel = 3;

    /** 球半径（cm）。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess",
              meta = (ClampMin = "100.0"))
    float GlobeRadiusCM = 15000.0f;

    /** HISM hover 状态 cell 边缘颜色（RGB）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|R11 Highlight")
    FLinearColor HighlightHoverColor = FLinearColor(1.0f, 0.85f, 0.10f, 1.0f);

    /** HISM 材质边缘高亮强度倍率。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|R11 Highlight",
              meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float HighlightStrength = 1.50f;

    /** Deprecated compatibility hook. HISM highlight now uses per-instance custom data. */
    void SetHighlightLUT(class UTexture2D* InLUT);

    //----------------------------------------------------------
    // SimpleGameplay：WorldGen 三地形参数
    //----------------------------------------------------------

    /**
     * SimpleGameplay WorldGen 参数（RandomSeed / 山脉数量与平均节点数 / 森林数量与平均节点数）。
     * 改动后 OnConstruction 自动重跑 Generator->Generate() 并刷新 HISM tile instances。
     * 与 APlanetTopologyDebugMesh 的 WorldGenSettings 字段语义完全一致（两 actor 各跑各自的实例）。
     */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|WorldGen")
    FWorldGenSettings WorldGenSettings;

    //----------------------------------------------------------
    // SimpleGameplay：HISM 静态网格瓦片渲染
    //----------------------------------------------------------

    /** true：主视觉使用每 Cell 一个 StaticMesh 实例的 HISM 瓦片渲染。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    bool bEnableHISMTileRendering = true;

    /** true：HISM 组件开启 QueryOnly 碰撞，可直接参与鼠标拾取。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    bool bEnableHISMTileCollision = true;

    /** 平原 Cell 使用的烘焙 StaticMesh 资产。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    TObjectPtr<UStaticMesh> PlainTileStaticMesh;

    /** 森林 Cell 使用的烘焙 StaticMesh 资产。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    TObjectPtr<UStaticMesh> ForestTileStaticMesh;

    /** 山脉 Cell 使用的烘焙 StaticMesh 资产。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    TObjectPtr<UStaticMesh> MountainTileStaticMesh;

    /** 生成瓦片资产时使用的 BaseRadius。默认 100，与 TerraSphericalTileGenerator 默认值一致。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles", meta = (ClampMin = "1.0"))
    float HISMTileSourceRadiusCM = 100.0f;

    /** HISM 瓦片相对 GlobeRadiusCM 的额外半径偏移。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    float HISMTileRadiusOffsetCM = 0.0f;

    /** HISM 瓦片额外统一缩放。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles", meta = (ClampMin = "0.001"))
    float HISMTileAdditionalUniformScale = 1.0f;

    /** true：启用 HISM 实例拾取与 PerInstanceCustomData tile 边缘高亮。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|HISM Highlight")
    bool bEnableHISMInstanceHighlight = true;

    /** HISM hover 离开球体后的防抖保留时间；语义与旧 CellHighlightComponent 保持一致。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|HISM Highlight",
              meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float HISMHoverFadeDuration = 0.5f;

    /** 材质侧 UV 边缘高亮内半径建议值；C++ 会写入 HISM 动态材质参数。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|HISM Highlight",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HISMHighlightInnerRadius = 0.20f;

    /** 材质侧 UV 边缘高亮外半径建议值；C++ 会写入 HISM 动态材质参数。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|HISM Highlight",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HISMHighlightOuterRadius = 0.50f;

    //----------------------------------------------------------
    // SimpleGameplay G1：棋子初始化调试绘制
    //----------------------------------------------------------

    /** true：用 DrawDebugSphere 显示 G1 初始棋子布局。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|SimpleGameplay G1")
    bool bEnableG1DebugPieces = true;

    /** G1 调试棋子球半径（cm）。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|SimpleGameplay G1", meta = (ClampMin = "10.0", ClampMax = "1000.0"))
    float G1DebugPieceRadiusCM = 140.0f;

    /** G1 调试棋子相对球面外抬高度（cm）。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|SimpleGameplay G1", meta = (ClampMin = "0.0", ClampMax = "5000.0"))
    float G1DebugPieceHeightOffsetCM = 200.0f;

    //----------------------------------------------------------
    // SimpleGameplay P1：真实棋子模型表现
    //----------------------------------------------------------

    /** true：PIE / 游戏运行时启用真实棋子 Actor 表现。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    bool bEnableP1PiecePresentation = true;

    /** true：P1 真实棋子启用时隐藏 G1 调试球，避免模型与球重叠。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    bool bHideG1DebugPiecesWhenP1IsActive = true;

    /** P1 棋子模型相对球面的外抬高度（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
    float P1PieceRadiusOffsetCM = 0.0f;

    /** P1 所有人物模型统一缩放。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P1PieceUniformScale = 4.0f;

    /** P1 模型组件相对棋子 Actor 根节点的位置修正。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FVector P1MeshRelativeLocation = FVector::ZeroVector;

    /** P1 模型组件相对棋子 Actor 根节点的导入朝向修正。默认 Yaw=90，用于修正当前人物模型"逻辑朝前实际朝右"的资源坐标差异。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FRotator P1MeshRelativeRotation = EditorXYZRotator(0.0f, 0.0f, -90.0f);

    /** 主将模型。推荐挂 Content/Animations/Adventurers/Characters/Mage1。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<USkeletalMesh> P1CommanderMesh;

    /** 步兵模型。推荐挂 Content/Animations/Adventurers/Characters/Knight1。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<USkeletalMesh> P1InfantryMesh;

    /** 骑兵占位人物模型。P1 暂不挂马，推荐先挂 Knight1。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<USkeletalMesh> P1CavalryMesh;

    /** 弓兵模型。推荐挂 Content/Animations/Adventurers/Characters/Ranger1。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<USkeletalMesh> P1ArcherMesh;

    /** P2 待机动画。推荐 Rig_Medium_GeneralIdle_A。为空时只更新驱动状态，不强制播放动画。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P2IdleAnimation;

    /** P2 普通移动动画。推荐 Rig_Medium_MovementBasicWalking_A。为空时仍播放位移插值。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P2MoveAnimation;

    /** P2 跳跃动画。推荐 Rig_Medium_MovementBasicJump_Full_Short。为空时仍播放跳跃弧线。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P2JumpAnimation;

    /** P2 普通移动单段时长（秒）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P2MoveDurationSeconds = 0.35f;

    /** P2 跳跃单段时长（秒）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P2JumpDurationSeconds = 0.55f;

    /** P2 跳跃最高额外外抬高度（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
    float P2JumpHeightCM = 650.0f;

    /** P2.5：true 时通过 HISM 碰撞射线修正棋子 Actor 的球面高度。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    bool bEnableP2_5HISMPieceHeightTrace = true;

    /** P2.5：棋子高度射线从球面外侧额外多远开始（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "100000.0"))
    float P2_5PieceHeightTraceStartOffsetCM = 5000.0f;

    /** P2.5：棋子高度射线穿过球心后继续多远（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "100000.0"))
    float P2_5PieceHeightTracePastCenterOffsetCM = 1000.0f;

    /** P2.5：true 时输出棋子 HISM 高度射线诊断日志。验收后可关闭。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    bool bDebugP2_5HISMPieceHeightTrace = true;

    /** P2.6：骑兵高度射线相对 Cell 中心外法线的角度偏移（度）。0 表示沿用 P2.5 中心采样。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation",
              meta = (ClampMin = "0.0", ClampMax = "15.0", UIMin = "0.0", UIMax = "5.0"))
    float P2_6CavalryHeightTraceAngularOffsetDeg = 2.0f;

    /** P4.1 骑兵马模型。推荐 Content/Animations/Horse/Horse。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<USkeletalMesh> P4HorseMesh;

    /** P4.1 马 Idle 动画。推荐 HorseIdle。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P4HorseIdleAnimation;

    /** P4.2 马移动动画。推荐 HorseWalk。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P4HorseMoveAnimation;

    /** P4.2 马跳跃动画。推荐 HorseGallop_Jump。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P4HorseJumpAnimation;

    /** P4.2 马跳跃动画播放速率缩放。最终速率 = 动画长度 / P2 跳跃时长 * 本参数。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P4HorseJumpPlayRateScale = 0.9f;

    /** P4.3 马死亡动画。推荐 HorseDeath。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P4HorseDeathAnimation;

    /** P4.1 骑手坐姿 Idle。推荐 Rig_Medium_GeneralSitting。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P4RiderSittingAnimation;

    /** P4.4 Rider 专用 AnimBP 类。父类必须是 TerraMountedRiderAnimInstance。为空时回退到 P4.3 直接 PlayAnimation。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TSubclassOf<UAnimInstance> P4RiderAnimInstanceClass;

    /** P4.4 Rider 上半身攻击 Montage。Slot 名建议 MountedUpperBody。为空时回退到 P3CavalryMeleeAttackAnimation 整身播放。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimMontage> P4RiderUpperBodyAttackMontage;

    /** P4.4 Rider 上半身受击 Montage。Slot 名建议 MountedUpperBody。为空时回退到 P3HitAnimation 整身播放。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimMontage> P4RiderUpperBodyHitMontage;

    /** P4.5 RiderAnchor 挂到 HorseMesh 的骨骼或 Socket 名。默认 Torso；若你在 Torso 上创建 SaddleSocket，可改成 SaddleSocket。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FName P4SaddleAttachName = TEXT("Torso");

    /** P4.1 马相对骑兵 Actor 根节点的位置修正。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FVector P4HorseRelativeLocation = FVector::ZeroVector;

    /** P4.1 马相对骑兵 Actor 根节点的导入朝向修正。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FRotator P4HorseRelativeRotation = EditorXYZRotator(0.0f, 0.0f, -90.0f);

    /** P4.1 马模型统一缩放。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P4HorseUniformScale = 3.0f;

    /** P4.1 骑手相对 RiderAnchor / 马背锚点的位置。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FVector P4RiderRelativeLocation = FVector(0.0f, -1.0f, 0.5f);

    /** P4.1 骑手相对 RiderAnchor / 马背锚点的旋转。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FRotator P4RiderRelativeRotation = EditorXYZRotator(0.0f, 0.0f, -90.0f);

    /** P4.1 骑手相对缩放。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P4RiderUniformScale = 0.003f;

    /** P4.3 骑兵死亡时 RiderAnchor 的落地相对位置。用于让 Rider 从马背落到地面再播放死亡动画。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FVector P4RiderDeathRelativeLocation = FVector::ZeroVector;

    /** P4.3 骑兵死亡时 RiderAnchor 的落地相对旋转。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FRotator P4RiderDeathRelativeRotation = FRotator::ZeroRotator;

    /** P5 弓兵弓 StaticMesh。推荐 Content/Animations/Adventurers/Assets/bow。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UStaticMesh> P5ArcherBowMesh;

    /** P5 步兵剑 StaticMesh。推荐 Content/Animations/Adventurers/Assets/sword_1handed。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UStaticMesh> P5InfantrySwordMesh;

    /** P5 步兵盾 StaticMesh。推荐 Content/Animations/Adventurers/Assets/shield_round。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UStaticMesh> P5InfantryShieldMesh;

    /** P5 骑兵斧 StaticMesh。推荐 Content/Animations/Adventurers/Assets/axe_1handed。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UStaticMesh> P5CavalryAxeMesh;

    /** P5 弓兵弓挂载 Socket / Bone 名。默认 handslot_l；为空时挂到 HumanMesh 根。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FName P5ArcherBowAttachName = TEXT("handslot_l");

    /** P5 步兵剑挂载 Socket / Bone 名。默认 handslot_r；为空时挂到 HumanMesh 根。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FName P5InfantrySwordAttachName = TEXT("handslot_r");

    /** P5 步兵盾挂载 Socket / Bone 名。默认 handslot_l；为空时挂到 HumanMesh 根。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FName P5InfantryShieldAttachName = TEXT("handslot_l");

    /** P5 骑兵斧挂载 Socket / Bone 名。默认 handslot_r；为空时挂到 RiderMesh 根。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FName P5CavalryAxeAttachName = TEXT("handslot_r");

    /** P5 弓兵弓相对挂点位置。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FVector P5ArcherBowRelativeLocation = FVector::ZeroVector;

    /** P5 弓兵弓相对挂点旋转。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FRotator P5ArcherBowRelativeRotation = EditorXYZRotator(0.0f, 180.0f, 0.0f);

    /** P5 弓兵弓相对缩放。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P5ArcherBowUniformScale = 0.01f;

    /** P5 步兵剑相对挂点位置。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FVector P5InfantrySwordRelativeLocation = FVector::ZeroVector;

    /** P5 步兵剑相对挂点旋转。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FRotator P5InfantrySwordRelativeRotation = FRotator::ZeroRotator;

    /** P5 步兵剑相对缩放。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P5InfantrySwordUniformScale = 0.01f;

    /** P5 步兵盾相对挂点位置。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FVector P5InfantryShieldRelativeLocation = FVector::ZeroVector;

    /** P5 步兵盾相对挂点旋转。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FRotator P5InfantryShieldRelativeRotation = EditorXYZRotator(0.0f, 90.0f, 180.0f);

    /** P5 步兵盾相对缩放。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P5InfantryShieldUniformScale = 0.01f;

    /** P5 骑兵斧相对挂点位置。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FVector P5CavalryAxeRelativeLocation = FVector::ZeroVector;

    /** P5 骑兵斧相对挂点旋转。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FRotator P5CavalryAxeRelativeRotation = FRotator::ZeroRotator;

    /** P5 骑兵斧相对缩放。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P5CavalryAxeUniformScale = 0.015f;

    /** P6 弓兵远程投射物 StaticMesh。默认 Content/Animations/Adventurers/Assets/arrow_bow。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UStaticMesh> P6ArrowProjectileMesh;

    /** P6 主将法术临时 Actor 类。默认 Content/FXVarietyPack/Blueprints/BP_ky_fireBall。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TSubclassOf<AActor> P6SpellProjectileActorClass;

    /** P6 箭矢释放挂点 Socket / Bone 名。默认 handslot_r；找不到时从棋子 Actor 当前位置释放。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FName P6ArrowAttachName = TEXT("handslot_r");

    /** P6 法术释放挂点 Socket / Bone 名。默认 handslot_r；找不到时从棋子 Actor 当前位置释放。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FName P6SpellAttachName = TEXT("handslot_r");

    /** P6 箭矢相对挂点位置。用于校准从弓弦 / 手部释放的位置。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FVector P6ArrowRelativeLocation = FVector::ZeroVector;

    /** P6 箭矢相对飞行朝向的旋转。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FRotator P6ArrowRelativeRotation = EditorXYZRotator(0.0f, 0.0f, -90.0f);

    /** P6 箭矢相对缩放。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P6ArrowUniformScale = 5.0f;

    /** P6 箭矢落点相对被吃 Actor 坐标系的位置偏移。用于校准箭矢落在身体中心、上身或脚下。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FVector P6ArrowTargetRelativeLocation = FVector::ZeroVector;

    /** P6 法术 Actor 相对挂点位置。用于校准从手部释放的位置。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FVector P6SpellRelativeLocation = FVector::ZeroVector;

    /** P6 法术 Actor 相对飞行朝向的旋转。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FRotator P6SpellRelativeRotation = EditorXYZRotator(0.0f, 90.0f, 0.0f);

    /** P6 法术 Actor 相对缩放。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P6SpellUniformScale = 1.0f;

    /** P6 箭矢释放延迟（秒）。从弓兵攻击动画起播算起，用于对齐松弦帧。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P6ArrowReleaseDelaySeconds = 0.5f;

    /** P6 法术释放延迟（秒）。从主将施法动画起播算起，用于对齐释放帧。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P6SpellReleaseDelaySeconds = 0.18f;

    /** P6 箭矢飞行时长（秒）。箭矢按测地线基础叠加抛物线高度飞向目标。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P6ArrowFlightSeconds = 0.5f;

    /** P6 法术飞行时长（秒）。法术按球面测地线飞向目标。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P6SpellFlightSeconds = 0.35f;

    /** P6 箭矢抛物线最大额外高度（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
    float P6ArrowArcHeightCM = 1500.0f;

    /** P7 阵营换色母材质。为空时尝试加载 /Game/PiecePresentation/Materials/M_TerraPiece_PaletteReplace。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P7 Faction Palette")
    TObjectPtr<UMaterialInterface> P7PaletteReplaceMaterial;

    /** P7 阵营颜色表。为空时 BuildP1PieceVisualConfig_ 使用内置 12 阵营默认色。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P7 Faction Palette")
    TArray<FTerraPieceFactionPalette> P7FactionPalettes;

    /** P7 主将基础贴图。默认 mage_texture。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P7 Faction Palette")
    TObjectPtr<UTexture2D> P7CommanderBaseTexture;

    /** P7 步兵基础贴图。默认 knight_texture。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P7 Faction Palette")
    TObjectPtr<UTexture2D> P7InfantryBaseTexture;

    /** P7 骑兵 Rider 基础贴图。默认 knight_texture；马本体首版不换色。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P7 Faction Palette")
    TObjectPtr<UTexture2D> P7CavalryRiderBaseTexture;

    /** P7 弓兵基础贴图。默认 ranger_texture。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P7 Faction Palette")
    TObjectPtr<UTexture2D> P7ArcherBaseTexture;

    /** P7 各兵种 UV 色块 mask。为空时使用已验证首版：Mage R1C2/R1C1，Knight R1C0/R1C1，Ranger R1C0/R1C6。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P7 Faction Palette")
    TArray<FTerraPiecePaletteMask> P7PaletteMasksByPieceType;

    /** P3.5 主将远程攻击动画目标播放时长（秒）。最终速率 = 动画剩余长度 / 本时长 * P35CommanderAttackPlayRateScale。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P35CommanderAttackDurationSeconds = 0.35f;

    /** P3.5 主将远程攻击动画播放速率缩放，用于微调施法动作和法术释放/飞行/受击同步。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P35CommanderAttackPlayRateScale = 1.0f;

    /** P3.5 弓兵远程攻击动画目标播放时长（秒）。最终速率 = 动画剩余长度 / 本时长 * P35ArcherAttackPlayRateScale。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P35ArcherAttackDurationSeconds = 0.35f;

    /** P3.5 弓兵远程攻击动画播放速率缩放，用于微调射箭动作和箭矢释放/飞行/受击同步。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P35ArcherAttackPlayRateScale = 0.3f;

    /** P3 主将施法攻击动画。推荐 Rig_Medium_GeneralMagic_Spell_Casting。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P3CommanderMagicAttackAnimation;

    /** P3 弓兵射击攻击动画。推荐 Rig_Medium_GeneralShooting_Arrow。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P3ArcherRangedAttackAnimation;

    /** P3 步兵近战攻击动画。推荐 Rig_Medium_GeneralSword_And_Shield_Slash。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P3InfantryMeleeAttackAnimation;

    /** P3 骑兵占位近战攻击动画。推荐 Rig_Medium_GeneralUpward_Thrust。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P3CavalryMeleeAttackAnimation;

    /** P3 通用受击动画。推荐 Rig_Medium_GeneralHit_A。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P3HitAnimation;

    /** P3 通用死亡动画。推荐 Rig_Medium_GeneralDeath_A。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P3DeathAnimation;

    /** P3 攻击前面向目标的 slerp 时长（秒）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float P3FacingBlendSeconds = 0.15f;

    /** P3 步兵 / 骑兵跑入被吃棋子 Cell 的时长（秒）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "5.0"))
    float P3MeleeRunInSeconds = 0.25f;

    /** P3 步兵 / 骑兵攻击后跑回原位的时长（秒）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "5.0"))
    float P3MeleeReturnSeconds = 0.25f;

    /** P3 攻击动画起播偏移（秒），用于校准有效攻击帧。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3AttackAnimationStartOffsetSeconds = 0.0f;

    /** P3 主将攻击动画起播后多久对齐到受击开始（秒）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3CommanderAttackToHitSeconds = 0.35f;

    /** P3 弓兵攻击动画起播后多久对齐到受击开始（秒）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3ArcherAttackToHitSeconds = 0.35f;

    /** P3 步兵攻击动画起播后多久对齐到受击开始（秒）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3InfantryAttackToHitSeconds = 0.25f;

    /** P3 骑兵攻击动画起播后多久对齐到受击开始（秒）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3CavalryAttackToHitSeconds = 0.25f;

    /** P3 兼容旧参数：作为没有攻击者时的最小受击延迟；有攻击者时会与各兵种 AttackToHit 取最大值。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3HitReactDelaySeconds = 0.2f;

    /** P3 受击动画起播偏移（秒），用于校准受击帧。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3HitAnimationStartOffsetSeconds = 0.0f;

    /** P3 受击触发后多久触发死亡动画（秒）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3DeathAfterHitDelaySeconds = 0.2f;

    /** P3 死亡动画起播偏移（秒），用于校准死亡帧。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3DeathAnimationStartOffsetSeconds = 0.0f;

    /** P3 被吃棋子死亡后淡出时长（秒）。当前首版用缩放淡出后销毁。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float P3CapturedFadeSeconds = 0.35f;

    //----------------------------------------------------------
    // SimpleGameplay G2.5：视角与当前阵营提示
    //----------------------------------------------------------

    /** true：回合开始自动切到当前阵营大本营上方，选中棋子时自动旋转视角对准。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G2.5")
    bool bEnableG2_5CameraAssist = true;

    /** 回合开始时摄像机位于大本营球面外侧的额外高度（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G2.5", meta = (ClampMin = "0.0", ClampMax = "100000.0"))
    float G2_5TurnStartCameraHeightCM = 8000.0f;

    /** C2：true 时游戏开始硬设置到当前阵营活棋子的战区中心斜俯视。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay C2 Camera", meta = (DisplayName = "Enable C2 Game Start War Zone Camera"))
    bool bEnableC2GameStartWarZoneCamera = true;

    /** C2.5：true 时每次回合开始平滑把 C3 视角中心切到当前阵营战区中心。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay C2.5 Camera")
    bool bEnableC2_5TurnStartWarZoneFocusBlend = true;

    /** C2.5：回合开始平滑切换视角中心的 Blend 时长（秒），不改变当前距离。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay C2.5 Camera", meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float C2_5TurnStartFocusBlendSeconds = 0.45f;

    /** C2：已禁用。回合开始距离改由 C3InitialDistanceToFocusCM 控制。字段仅保留以兼容旧关卡序列化。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlanetTopology|Tess|SimpleGameplay C2 Camera|Deprecated", meta = (DeprecatedProperty, DeprecationMessage = "Disabled. Use C3InitialDistanceToFocusCM instead."))
    float C2TurnStartCameraDistanceCM = 18000.0f;

    /** C2：已禁用。回合开始倾角改由 C3.7 自动倾角逻辑根据距离派生。字段仅保留以兼容旧关卡序列化。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlanetTopology|Tess|SimpleGameplay C2 Camera|Deprecated", meta = (DeprecatedProperty, DeprecationMessage = "Disabled. C3.7 auto tilt derives tilt from distance."))
    float C2TurnStartCameraTiltDeg = 55.0f;

    /** 当前阵营所有棋子脚下 Cell 的淡粉色提示。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G2.5")
    FLinearColor G2_5CurrentFactionPieceColor = FLinearColor(1.0f, 0.45f, 0.68f, 1.0f);

    /** hover 到当前阵营棋子时的加红提示色。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G2.5")
    FLinearColor G2_5CurrentFactionPieceHoverColor = FLinearColor(1.0f, 0.22f, 0.32f, 1.0f);

    //----------------------------------------------------------
    // SimpleGameplay G3：跳跃与调试
    //----------------------------------------------------------

    /** true：调试时回合结束不跳到下一个玩家，下一回合仍保持当前玩家。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G3|Debug")
    bool bG3DebugKeepSameFactionOnEndTurn = false;

    /** hover 到可行走 / 可跳跃淡蓝落点时使用的加深颜色。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G3")
    FLinearColor G3ActionTargetHoverColor = FLinearColor(0.08f, 0.45f, 1.0f, 1.0f);

    /** hover 到某个可吃子落点时，该落点对应可吃目标使用的加深红色。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G4")
    FLinearColor G4CaptureTargetHoverColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);

    //----------------------------------------------------------
    // SimpleGameplay G8：手动视角轨道控制
    //----------------------------------------------------------

    /** true：允许 PlayerController 通过 WSAD + 滚轮驱动球面轨道相机。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G8")
    bool bEnableG8ManualCameraControl = true;

    /** G8：每秒经纬度变化速度（度 / 秒）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G8", meta = (ClampMin = "1.0", ClampMax = "360.0"))
    float G8CameraOrbitDegreesPerSecond = 45.0f;

    /** G8：每次滚轮缩放的高度步长（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G8", meta = (ClampMin = "10.0", ClampMax = "100000.0"))
    float G8CameraZoomStepCM = 800.0f;

    /** G8：相机允许的最小离地高度（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G8", meta = (ClampMin = "0.0", ClampMax = "100000.0"))
    float G8CameraMinHeightOffsetCM = 2500.0f;

    /** G8：相机允许的最大离地高度（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G8", meta = (ClampMin = "0.0", ClampMax = "200000.0"))
    float G8CameraMaxHeightOffsetCM = 30000.0f;

    /** C3.7：自动倾角插值的最小距离（cm）。低于此距离时倾角固定为 C3AutoTiltAtMinDistanceDeg。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G8", meta = (ClampMin = "0.0", ClampMax = "200000.0"))
    float C3AutoTiltMinDistanceCM = 6000.0f;

    /** C3.7：自动倾角插值的最大距离（cm）。高于此距离时倾角固定为 C3AutoTiltAtMaxDistanceDeg。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G8", meta = (ClampMin = "0.0", ClampMax = "200000.0"))
    float C3AutoTiltMaxDistanceCM = 20000.0f;

    /** C3.7：最小距离时的视线倾角（度）。玩家拉到最近时接近平视地表。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G8", meta = (ClampMin = "5.0", ClampMax = "85.0"))
    float C3AutoTiltAtMinDistanceDeg = 30.0f;

    /** C3.7：最大距离时的视线倾角（度）。玩家拉到最远时接近垂直俯瞰。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G8", meta = (ClampMin = "5.0", ClampMax = "85.0"))
    float C3AutoTiltAtMaxDistanceDeg = 85.0f;

    /** C3：聚焦相机手动模式的 fallback 初始距离（cm）。仅在首次进入手动模式且 Sync 尚未完成时使用。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G8", meta = (ClampMin = "0.0", ClampMax = "200000.0"))
    float C3InitialDistanceToFocusCM = 8000.0f;

    /** C4：true 时点击选中棋子会先判断是否需要智能聚焦。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay C4 Camera")
    bool bEnableC4SmartSelectionFocus = true;

    /** C4：选中单位与当前视角中心的球面角距离不超过该值时，认为已在舒适区内。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay C4 Camera", meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float C4ComfortFocusAngleDeg = 9.0f;

    /** C4：选中屏幕外/边缘棋子时 C3 焦点 Blend 时长（秒）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay C4 Camera", meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float C4SelectedPieceFocusBlendSeconds = 0.5f;

    /** C6：true 时棋子行动落点离开 C4 舒适区会同步触发行动镜头追踪。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay C6 Camera")
    bool bEnableC6ActionCameraTracking = true;

    /** C6.5：true 时行动确认换回合后，等待本次移动 / 攻击表现预计结束再执行 C2.5 回合战区回正。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay C6.5 Camera")
    bool bEnableC6_5DelayTurnStartFocusUntilActionPresentationEnds = true;

    /** C6.5：延迟回正额外缓冲秒数，避免计时略早于动画 / timer 结束。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay C6.5 Camera", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float C6_5TurnStartFocusDelayPaddingSeconds = 0.1f;

    //----------------------------------------------------------
    // 生命周期
    //----------------------------------------------------------
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
    virtual bool ShouldTickIfViewportsOnly() const override;
#endif

    // 注：不 override BeginDestroy()。
    // TUniquePtr fields are released by ~APlanetTessellatedMesh() in .cpp where
    // their complete types are visible; early Reset() from BeginDestroy risks
    // fighting UObject GC/editor teardown ordering.

    /**
     * 重建 CellTopology、WorldGen、HISM tile instances 与 SimpleGameplay 状态。
     * Editor 中拖入关卡或修改属性时（OnConstruction）会自动调用。
     */
    UFUNCTION(CallInEditor, Category = "PlanetTopology|Tess")
    void Rebuild();

    /** 尝试把鼠标命中的 HISM 实例解析成 CellId。 */
    UFUNCTION(BlueprintCallable, Category = "PlanetTopology|Tess|HISM Highlight")
    bool TryResolveHISMHitToCellId(const FHitResult& Hit, int32& OutCellId) const;

    /** 输入层 hover 命中 HISM 时调用；成功处理返回 true。 */
    bool HandleHISMHoverHit(const FHitResult& Hit);

    /** 输入层 click 命中 HISM 时调用；成功处理返回 true。 */
    bool HandleHISMClickHit(const FHitResult& Hit);

    /** 输入层 Tab / Shift+Tab 循环当前阵营可行动棋子时调用；成功处理返回 true。 */
    bool HandleC5NavigateCurrentFactionPiece(bool bReverse);

    /** 输入层右键撤销当前未提交行动时调用；成功处理返回 true。 */
    bool HandleHISMUndo();

    /** 鼠标离开 HISM 瓦片时调用，只清 hover，不清 select。 */
    void ClearHISMHover();

    /** 清空全部 HISM hover / select 高亮。 */
    UFUNCTION(BlueprintCallable, Category = "PlanetTopology|Tess|HISM Highlight")
    void ClearAllHISMHighlights();

    /** 获取当前 HISM hover CellId。 */
    UFUNCTION(BlueprintCallable, Category = "PlanetTopology|Tess|HISM Highlight")
    int32 GetLastHISMPickedCellId() const { return HISMTileRenderer.GetLastPickedCellId(); }

    /** 获取最近一次 HISM click CellId。 */
    UFUNCTION(BlueprintCallable, Category = "PlanetTopology|Tess|HISM Highlight")
    int32 GetLastHISMClickedCellId() const { return HISMTileRenderer.GetLastClickedCellId(); }

private:
    /** 整体重建：Cell 拓扑 + WorldGen + HISM Tiles + Gameplay。 */
    void RebuildAll_();

    /** 私有：构造（或重建）玩法 / WorldGen 共用的 Cell 拓扑。 */
    void RebuildTopologies_();

    /** 私有：输出 Cell 拓扑统计到 Output Log。 */
    void LogTopologyStats_() const;

    /** SimpleGameplay：按 WorldGen 三地形输出重建平原 / 森林 / 山脉三套 HISM 实例。 */
    void RebuildHISMTileInstances_();

    FPlanetHISMTileRenderConfig BuildHISMTileRenderConfig_() const;
    FPlanetHISMHighlightConfig BuildHISMHighlightConfig_() const;

    /** SimpleGameplay：统一应用 HISM 显示 / 碰撞开关。 */
    void ApplyRenderModeVisibility_();

    /** SimpleGameplay：把单个 Cell 当前 Gameplay/hover 逻辑合成为最终 RGB + Intensity 自定义数据。 */
    void WriteHISMHighlightForCell_(int32 CellId, bool bMarkRenderStateDirty = true);
    void RefreshG4CapturePreviewCellsForActionTarget_(int32 ActionTargetCellId, bool bMarkLastRenderStateDirty = true);
    void UpdateHISMHoverCell_(int32 NewCellId);

    void RebuildGameplay_();

    /** SimpleGameplay G2：刷新 Gameplay 容器报告的脏 Cell 高亮。 */
    void RefreshGameplayHighlights_(const TArray<int32>& DirtyCellIds);

    /** SimpleGameplay G2/C5：统一处理 Gameplay Cell 点击及其表现侧后处理。 */
    bool HandleGameplayCellClick_(int32 CellId, const TCHAR* SourceLabel, int32 InstanceIndex, const FString& ComponentName);

    /** SimpleGameplay A3：NPC MCP 执行入口，复用真实点击路径以同步表现层。 */
    bool TryExecuteNpcMcpValidatedAction_(int32 ExpectedTurnIndex, int32 ExpectedFactionId, int32 PieceId, int32 ToCellId, FTerraGameplayContainer::FValidatedActionExecutionResult& OutResult);

    /** SimpleGameplay G2.5：刷新指定阵营所有棋子所在 Cell 的 HISM 高亮。 */
    void RefreshFactionPieceHighlights_(int32 FactionId);

    /** SimpleGameplay G2.5：刷新当前阵营所有棋子所在 Cell 的 HISM 高亮。 */
    void RefreshCurrentFactionPieceHighlights_();

    /** SimpleGameplay G2.5：根据 CellId 计算球面 Cell 中心世界坐标。 */
    bool GetCellSurfaceWorldPosition_(int32 CellId, float RadiusOffsetCM, FVector& OutWorldPosition) const;

    /** SimpleGameplay C6.5：timer 到点后校验仍是同一回合，再调用既有 C2/C2.5 回合镜头入口。 */
    void ExecuteC6_5DelayedTurnStartFocus_(int32 ExpectedTurnIndex, int32 ExpectedFactionId);

    /** SimpleGameplay P1/P2：根据当前 Gameplay 快照增量同步真实棋子 Actor，可选播放 P2 移动事件。 */
    void SyncP1PiecePresentation_(
        const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents = TArray<FTerraPiecePresentationMoveEvent>(),
        const TArray<FTerraPiecePresentationCaptureEvent>& CaptureEvents = TArray<FTerraPiecePresentationCaptureEvent>());

    /** SimpleGameplay P1：清空真实棋子 Actor 表现。 */
    void ClearP1PiecePresentation_();

    /** SimpleGameplay P1：把 CellId 转为棋子 Actor 的球面世界 Transform。 */
    bool BuildP1PieceWorldTransform_(int32 CellId, FTransform& OutWorldTransform) const;
    bool BuildP1PieceWorldTransform_(int32 CellId, ETerraGameplayPieceType PieceType, FTransform& OutWorldTransform) const;

    /** SimpleGameplay P2.5/P2.6：从采样方向向球心射线命中 HISM，以命中半径修正棋子中心高度。 */
    bool TryResolveP2_5PieceHeightFromHISM_(int32 CellId, const FVector& TraceWorldUp, const FVector& PlacementWorldUp, float TraceAngularOffsetDeg, FVector& OutWorldPosition) const;

    /** SimpleGameplay P1/P2：按"背向本阵营主将"规则计算初始棋子朝向。 */
    bool BuildP1PieceWorldTransformForPiece_(const FTerraGameplayPieceState& Piece, const TArray<FTerraGameplayPieceState>& Pieces, FTransform& OutWorldTransform) const;

    /** SimpleGameplay P3：把 Gameplay 待结算吃子条目转换成表现事件。 */
    void BuildP3CaptureEventsFromPendingEntries_(
        const TArray<FTerraGameplayCaptureEntry>& CaptureEntries,
        const TArray<FTerraGameplayPieceState>& PiecesBeforeResolution,
        TArray<FTerraPiecePresentationCaptureEvent>& OutCaptureEvents) const;

    /** SimpleGameplay P1：从 Details 面板资产槽生成表现配置。 */
    FTerraPieceVisualConfig BuildP1PieceVisualConfig_() const;

public:
    /** G8：取球心世界坐标。 */
    FVector GetPlanetCenterWorld_() const;

    /** G8：取球半径（cm）。 */
    float GetPlanetRadiusCM_() const { return GlobeRadiusCM; }

    /** G8：把某个世界位置同步成球面轨道参数。C3 后仅保留兼容旧路径。 */
    bool SyncOrbitCameraStateFromWorldPosition(const FVector& CameraWorldPosition, float& InOutLongitudeDeg, float& InOutLatitudeDeg, float& InOutHeightOffsetCM) const;

    /** G8：按球面轨道参数把视角应用到当前 PlayerController / ViewTarget。C3 后仅保留兼容旧路径。 */
    bool ApplyOrbitCameraState(float LongitudeDeg, float LatitudeDeg, float HeightOffsetCM);

    /**
     * C3：从当前相机位置和旋转反推焦点式手动相机状态。仅在进入手动模式的首帧被调用一次；
     * 后续帧不再调用（详见 C3FocusCameraManualControlDesign.md §4.2）。
     */
    bool SyncFocusCameraStateFromView(
        const FVector& CameraWorldPosition,
        const FRotator& CameraWorldRotation,
        FVector& OutFocusUnitDir,
        float& OutDistanceToFocusCM,
        float& OutTiltDeg,
        float& OutYawAroundFocusDeg) const;

    /** C3：按焦点式相机状态把视角应用到当前 PlayerController / ViewTarget。 */
    bool ApplyFocusCameraState(
        const FVector& FocusUnitDir,
        float DistanceToFocusCM,
        float TiltDeg,
        float YawAroundFocusDeg);

    /**
     * C3：在当前焦点局部切平面上沿测地线（大圆）推进焦点，同时用 Rodrigues 旋转把 Yaw
     * 平行运输到新焦点。这样连续按住 A/D 或 W/S 会走一整条大圆，不会退化成纬线。
     * 详见 C3FocusCameraManualControlDesign.md §5.1.3。
     */
    bool OffsetFocusCameraStateOnTangent(
        float RightDeltaDeg,
        float ForwardDeltaDeg,
        FVector& InOutFocusUnitDir,
        float& InOutYawAroundFocusDeg) const;

    /** SimpleGameplay G1/G2：按当前 Gameplay 棋子状态重建调试棋子缓存。 */
    void RebuildG1DebugPieces_();

    /** SimpleGameplay G1：用 DrawDebugSphere 绘制当前调试棋子缓存。 */
    void DrawG1DebugPieces_() const;

    /** D15：PIE 退出后材质恢复钩子（详见 AgentWorkflow.md §3.11）。 */
    void OnPostWorldCleanup_(class UWorld* World, bool bSessionEnded, bool bCleanupResources);

    //----------------------------------------------------------
    // 运行时持有
    //----------------------------------------------------------

    /** 逻辑层 sub=CellSubdivisionLevel 拓扑实例（玩法 / WorldGen 共用）。 */
    TUniquePtr<FSphereTopology> CellTopology;

    /**
     * WorldGen 主类实例。Rebuild() 中按顺序：
     *   RebuildTopologies_ → MakeUnique<FWorldGenerator>(CellTopology, Settings) → Generate()
     *     → RebuildHISMTileInstances_ → RebuildGameplay_。
     */
    TUniquePtr<FWorldGenerator> Generator;

    /** SimpleGameplay：平原瓦片 HISM 组件。 */
    UPROPERTY(VisibleAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PlainTileHISMComp;

    /** SimpleGameplay：森林瓦片 HISM 组件。 */
    UPROPERTY(VisibleAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ForestTileHISMComp;

    /** SimpleGameplay：山脉瓦片 HISM 组件。 */
    UPROPERTY(VisibleAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> MountainTileHISMComp;

    /** SimpleGameplay P1：真实棋子模型表现管理器。 */
    UPROPERTY(VisibleAnywhere, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UTerraPiecePresentationManager> PiecePresentationManager;

    /** HISM 瓦片渲染、实例索引、命中反查与 PerInstanceCustomData 高亮状态。 */
    FPlanetHISMTileRenderer HISMTileRenderer;

    /** SimpleGameplay G2：棋子、Cell 逻辑、回合和 Gameplay 高亮的总容器。 */
    TUniquePtr<FTerraGameplayContainer> GameplayContainer;

    /** SimpleGameplay G2.5：上一次已刷新底色提示的当前阵营。 */
    int32 G2_5LastHighlightedFactionId = INDEX_NONE;

    /** SimpleGameplay Camera：镜头行为与瞬态状态。配置字段暂留本 Actor 以兼容关卡序列化。 */
    FPlanetCameraController CameraController;

    /** SimpleGameplay G1/G2：当前初始化出的调试棋子缓存。 */
    TArray<FTerraG1DebugPiece> G1DebugPieces;

    //----------------------------------------------------------
    // D15：PIE 退出后材质恢复（详见 AgentWorkflow.md §3.11）
    //----------------------------------------------------------
    /** FWorldDelegates::OnPostWorldCleanup 委托句柄（构造函数末尾注册，析构中解绑）。 */
    FDelegateHandle PostWorldCleanupHandle;
};
