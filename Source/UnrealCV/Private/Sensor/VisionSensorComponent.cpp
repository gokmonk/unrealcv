// Fill out your copyright notice in the Description page of Project Settings.

#include "UnrealCVPrivate.h"
#include "VisionSensorComponent.h"
// #include "GlobalShader.h"
// #include "Shader.h"
// #include "ScreenRendering.h"
// #include "SceneRendering.h"
// #include "ScenePrivate.h"
// #include "ClearQuad.h"
// #include "JsonObjectConverter.h"
#include "TextureReader.h"
#include "FileHelper.h"

DECLARE_CYCLE_STAT(TEXT("ReadBuffer"), STAT_ReadBuffer, STATGROUP_UnrealCV);
DECLARE_CYCLE_STAT(TEXT("ReadBufferFast"), STAT_ReadBufferFast, STATGROUP_UnrealCV);
// DECLARE_CYCLE_STAT(TEXT("ReadPixels"), STAT_ReadPixels, STATGROUP_UnrealCV);

FImageWorker UVisionSensorComponent::ImageWorker;

UVisionSensorComponent::UVisionSensorComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer), Width(640), Height(480)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> EditorCameraMesh(TEXT("/Engine/EditorMeshes/MatineeCam_SM"));
	CameraMesh = EditorCameraMesh.Object;
	// From UCameraComponent
	// static ConstructorHelpers::FObjectFinder<UStaticMesh> EditorCameraMesh(TEXT("/Engine/EditorMeshes/MatineeCam_SM"));
	// CameraMesh = EditorCameraMesh.Object;

	// bool bTransient = true;
	// TextureTarget = ObjectInitializer.CreateDefaultSubobject<UTextureRenderTarget2D>(this, TEXT("TextureRenderTarget2D"));
	// TextureTarget->InitAutoFormat(640, 480);


}

void UVisionSensorComponent::OnRegister()
{
	Super::OnRegister();

	TextureTarget = NewObject<UTextureRenderTarget2D>(this);
	bool bUseLinearGamma = false;
	TextureTarget->InitCustomFormat(Width, Height, EPixelFormat::PF_B8G8R8A8, bUseLinearGamma);
	this->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	this->bCaptureEveryFrame = true; // TODO: Check the performance overhead for this
	this->bCaptureOnMovement = false;

	ImageWorker.Start();

	// Add a visualization camera mesh
	if (GetOwner()) // Check whether this is a template project
	// if (!IsTemplate())
	{
		ProxyMeshComponent = NewObject<UStaticMeshComponent>(GetOwner(), NAME_None, RF_Transactional | RF_TextExportTransient);
		// ProxyMeshComponent = this->CreateDefaultSubobject<UStaticMeshComponent>("Visualization Camera Mesh");
		ProxyMeshComponent->SetupAttachment(this);
		ProxyMeshComponent->bIsEditorOnly = true;
		ProxyMeshComponent->SetStaticMesh(CameraMesh);
		ProxyMeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		ProxyMeshComponent->bHiddenInGame = true;
		ProxyMeshComponent->CastShadow = false;
		ProxyMeshComponent->PostPhysicsComponentTick.bCanEverTick = false;
		ProxyMeshComponent->CreationMethod = CreationMethod;
		ProxyMeshComponent->RegisterComponentWithWorld(GetWorld());
	}

}


// Serialize the data to png and npy, check the speed.

// EPixelFormat::PF_B8G8R8A8
/*
This is defined in FColor
	#ifdef _MSC_VER
	// Win32 x86
	union { struct{ uint8 B,G,R,A; }; uint32 AlignmentDummy; };
#else
	// Linux x86, etc
	uint8 B GCC_ALIGN(4);
	uint8 G,R,A;
*/

void UVisionSensorComponent::GetLit(TArray<FColor>& ImageData, int& Width, int& Height)
{
	TFunction<void(FColor*, int32, int32)> Callback = [&](FColor* ColorPtr, int InWidth, int InHeight)
	{
		// Copy data to ImageData
		Width = InWidth;
		Height = InHeight;

		ImageData.AddZeroed(Width * Height);
		FColor* DestPtr = ImageData.GetData();
		for (int32 Row = 0; Row < Height; ++Row)
		{
			FMemory::Memcpy(DestPtr, ColorPtr, sizeof(FColor)*Width);
			ColorPtr += Width;
			DestPtr += Width;
		}
	};
	FTextureRenderTargetResource* RenderTargetResource = this->TextureTarget->GameThread_GetRenderTargetResource();
	FTexture2DRHIRef Texture2D = RenderTargetResource->GetRenderTargetTexture();
	FastReadTexture2DAsync(Texture2D, Callback);
	FlushRenderingCommands(); // Ensure the callback is done
}


void UVisionSensorComponent::GetLitAsync(const FString& Filename)
{
	auto Callback = [=](FColor* ColorBuffer, int Width, int Height)
	{
		// Initialize the image data array
		TArray<FColor> Dest; // Write this.
		Dest.AddZeroed(Width * Height); // TODO: will break!
		FColor* DestPtr = Dest.GetData();

		for (int32 Row = 0; Row < Height; ++Row)
		{
			FMemory::Memcpy(DestPtr, ColorBuffer, sizeof(FColor)*Width);
			ColorBuffer += Width;
			DestPtr += Width;
		}

		// ImageWorker.SaveFile(Dest, Width, Height, Filename);
		// ImageUtil.SaveBmpFile(Dest, Width, Height, FString::Printf(TEXT("%s_%d.bmp"), *Filename, GFrameNumber));
		ImageUtil.SaveBmpFile(Dest, Width, Height, Filename);
	};

	FTextureRenderTargetResource* RenderTargetResource = this->TextureTarget->GameThread_GetRenderTargetResource();
	FTexture2DRHIRef Texture2D = RenderTargetResource->GetRenderTargetTexture();
	FastReadTexture2DAsync(Texture2D, Callback);
}

void UVisionSensorComponent::GetLitSlow(TArray<FColor>& ImageData, int& Width, int& Height)
{
	SCOPE_CYCLE_COUNTER(STAT_ReadBuffer);

	UTextureRenderTarget2D* RenderTarget = this->TextureTarget;
	ReadTextureRenderTarget(RenderTarget, ImageData, Width, Height);
}

/** Get the location in unrealcv format */
FString UVisionSensorComponent::GetSensorWorldLocation()
{
	FVector ComponentLocation = this->GetComponentLocation();
	return ComponentLocation.ToString();
}

FString UVisionSensorComponent::GetSensorRotation()
{
	FRotator Rotation = this->GetComponentRotation();
	return Rotation.ToString();
}

FString UVisionSensorComponent::GetSensorPose()
{
	return TEXT("");
}



void UVisionSensorComponent::SetFOV(float FOV)
{
	this->FOVAngle = FOV;
}

void UVisionSensorComponent::SetPostProcessMaterial(UMaterial* PostProcessMaterial)
{
	PostProcessSettings.AddBlendable(PostProcessMaterial, 1);
}
