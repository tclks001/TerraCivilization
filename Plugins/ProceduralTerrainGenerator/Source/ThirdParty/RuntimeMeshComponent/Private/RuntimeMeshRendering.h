// Copyright 2016-2020 TriAxis Games L.L.C. All Rights Reserved.

#pragma once

#include "Engine/Engine.h"
#include "RuntimeMeshCore.h"
#include "RuntimeMeshRenderable.h"
#include "Containers/ResourceArray.h"
#include "RHIResourceReplace.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RuntimeMeshComponentPlugin.h"

class FRuntimeMeshVertexBuffer;
class FRuntimeMeshIndexBuffer;

class FRuntimeMeshBufferUpdateData : public FResourceArrayInterface
{
	int32 ElementStride;
	int32 NumElements;
	TArray<uint8> Data;

public:


	FRuntimeMeshBufferUpdateData(FRuntimeMeshVertexPositionStream&& InPositions)
		: ElementStride(InPositions.GetStride())
		, NumElements(InPositions.Num())
		, Data(MoveTemp(InPositions).TakeData())
	{

	}

	FRuntimeMeshBufferUpdateData(FRuntimeMeshVertexTangentStream&& InTangents)
		: ElementStride(InTangents.GetStride())
		, NumElements(InTangents.Num())
		, Data(MoveTemp(InTangents).TakeData())
	{

	}

	FRuntimeMeshBufferUpdateData(FRuntimeMeshVertexTexCoordStream&& InTexCoords)
		: ElementStride(InTexCoords.GetStride())
		, NumElements(InTexCoords.Num())
		, Data(MoveTemp(InTexCoords).TakeData())
	{

	}

	FRuntimeMeshBufferUpdateData(FRuntimeMeshVertexColorStream&& InColors)
		: ElementStride(InColors.GetStride())
		, NumElements(InColors.Num())
		, Data(MoveTemp(InColors).TakeData())
	{

	}

	FRuntimeMeshBufferUpdateData(FRuntimeMeshTriangleStream&& InTriangles)
		: ElementStride(InTriangles.GetStride())
		, NumElements(InTriangles.Num())
		, Data(MoveTemp(InTriangles).TakeData())
	{

	}

	virtual ~FRuntimeMeshBufferUpdateData() {}


	int32 GetStride() const { return ElementStride; }
	int32 GetNumElements() const { return NumElements; }

	const void* GetResourceData() const override
	{
		return reinterpret_cast<const void*>(Data.GetData());
	}
	uint32 GetResourceDataSize() const override
	{
		return Data.Num();
	}
	void Discard() override
	{
		Data.Empty();
	}
	bool IsStatic() const override
	{
		return false;
	}
	bool GetAllowCPUAccess() const override
	{
		return true;
	}
	void SetAllowCPUAccess(bool bInNeedsCPUAccess) override
	{
	}

};

class FRuntimeMeshSectionUpdateData
{
public:

	FRuntimeMeshBufferUpdateData Positions;
	FRuntimeMeshBufferUpdateData Tangents;
	FRuntimeMeshBufferUpdateData TexCoords;
	FRuntimeMeshBufferUpdateData Colors;

	FRuntimeMeshBufferUpdateData Triangles;
	FRuntimeMeshBufferUpdateData AdjacencyTriangles;




	FBufferRHIRef PositionsBuffer;
	FBufferRHIRef TangentsBuffer;
	FBufferRHIRef TexCoordsBuffer;
	FBufferRHIRef ColorsBuffer;

	FBufferRHIRef TrianglesBuffer;
	FBufferRHIRef AdjacencyTrianglesBuffer;


	const bool bHighPrecisionTangents : 1;
	const bool bHighPrecisionTexCoords : 1;
	const uint8 NumTexCoordChannels;
	const bool b32BitTriangles;
	const bool b32BitAdjacencyTriangles;


	bool bBuffersCreated;


	FRuntimeMeshSectionUpdateData(FRuntimeMeshRenderableMeshData&& InMesh)
		: Positions(MoveTemp(InMesh.Positions))
		, Tangents(MoveTemp(InMesh.Tangents))
		, TexCoords(MoveTemp(InMesh.TexCoords))
		, Colors(MoveTemp(InMesh.Colors))
		, Triangles(MoveTemp(InMesh.Triangles))
		, AdjacencyTriangles(MoveTemp(InMesh.AdjacencyTriangles))
		, bHighPrecisionTangents(InMesh.Tangents.IsHighPrecision())
		, bHighPrecisionTexCoords(InMesh.TexCoords.IsHighPrecision())
		, NumTexCoordChannels(InMesh.TexCoords.NumChannels())
		, b32BitTriangles(InMesh.Triangles.IsHighPrecision())
		, b32BitAdjacencyTriangles(InMesh.AdjacencyTriangles.IsHighPrecision())
		, bBuffersCreated(false)
	{

	}

	template<bool bIsInRenderThread>
	void CreateRHIBuffers(bool bShouldUseDynamicBuffers);

};

/** Single vertex buffer to hold one vertex stream within a section */
class FRuntimeMeshVertexBuffer : public FVertexBuffer
{
protected:

	/** Should this buffer by flagged as dynamic */
	const bool bIsDynamicBuffer;

	/** Size of a single vertex */
	int32 VertexSize;

	/** The number of vertices this buffer is currently allocated to hold */
	int32 NumVertices;

	/** Shader Resource View for this buffer */
	FShaderResourceViewRHIRef ShaderResourceView;

public:

	FRuntimeMeshVertexBuffer(bool bInIsDynamicBuffer, int32 DefaultVertexSize);

	~FRuntimeMeshVertexBuffer() {}

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	void ReleaseRHI() override;

	/** Gets the size of the vertex */
	FORCEINLINE int32 Stride() const { return VertexSize; }

	/** Get the size of the vertex buffer */
	FORCEINLINE int32 Num() const { return NumVertices; }

	/** Gets the full allocated size of the buffer (Equal to VertexSize * NumVertices) */
	FORCEINLINE int32 GetBufferSize() const { return NumVertices * VertexSize; }

	/** Gets the size of a single piece of data within an element */
	virtual int32 GetElementDatumSize() const = 0;

	/** Gets the format of the element, needed for creation of SRV's */
	virtual EPixelFormat GetElementFormat() const = 0;

	/** Binds the vertex buffer to the factory data type */
	virtual void Bind(FLocalVertexFactory::FDataType& DataType) = 0;

public:

	template<bool bIsInRenderThread>
	static FBufferRHIRef CreateRHIBuffer(FRuntimeMeshBufferUpdateData& InStream, bool bDynamicBuffer)
	{
		const uint32 SizeInBytes = InStream.GetResourceDataSize();

		// Basic validation/logging
		const int32 NumElements = InStream.GetNumElements();
		const int32 Stride = InStream.GetStride();
		const bool bIsNullStream = !(NumElements > 0 && Stride > 0);
		if (Stride > 0 && NumElements > 0)
		{
			if ((uint64)Stride * (uint64)NumElements != (uint64)SizeInBytes)
			{
				UE_LOG(RuntimeMeshLog, Warning, TEXT("CreateRHIBuffer(Vertex): size mismatch: elements=%d stride=%d computedSize=%llu actualSize=%u"), NumElements, Stride, (unsigned long long)Stride * (unsigned long long)NumElements, SizeInBytes);
			}
		}
		else
		{
			UE_LOG(RuntimeMeshLog, Verbose, TEXT("CreateRHIBuffer(Vertex): creating NULL buffer (elements=%d stride=%d size=%u dynamic=%d)"), NumElements, Stride, SizeInBytes, bDynamicBuffer);
		}

		FRHIBufferCreateDesc BufferDesc;

		if (!bIsNullStream)
		{
			// Create a vertex buffer for vertex streams
			BufferDesc = FRHIBufferCreateDesc::CreateVertex(TEXT("RuntimeMeshComponent"))
				.SetSize(SizeInBytes)
				.SetStride(Stride)
				.SetUsage((bDynamicBuffer ? BUF_Dynamic : BUF_Static) | BUF_VertexBuffer | BUF_ShaderResource)
				.SetInitialState(ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask)
				.SetInitActionResourceArray(&InStream);
		}
		else
		{
			// Ensure null buffers still have a sane initial state to avoid RHI validation issues
			BufferDesc = FRHIBufferCreateDesc::CreateNull(TEXT("RuntimeMeshComponent")).SetInitialState(ERHIAccess::VertexOrIndexBuffer);
		}

		UE_LOG(RuntimeMeshLog, Verbose, TEXT("CreateRHIBuffer(Vertex): creating buffer (elements=%d stride=%d size=%u dynamic=%d isNull=%d)"), NumElements, Stride, SizeInBytes, bDynamicBuffer, (int)bIsNullStream);

		FBufferRHIRef NewBuffer = GetImmediateCommandList().CreateBuffer(BufferDesc);

		if (NewBuffer.IsValid())
		{
			UE_LOG(RuntimeMeshLog, Verbose, TEXT("CreateRHIBuffer(Vertex): buffer created. Usage=%u"), (uint32)NewBuffer->GetUsage());
		}
		else
		{
			UE_LOG(RuntimeMeshLog, Warning, TEXT("CreateRHIBuffer(Vertex): CreateBuffer returned null!"));
		}

		return NewBuffer;
	}

	//template <uint32 MaxNumUpdates>
	void UpdateRHIFromExisting(FRHIBuffer* IntermediateBuffer, FRHIResourceReplaceBatcher& Batcher)
	{
		check(VertexBufferRHI && IntermediateBuffer);

		Batcher.EnqueueReplace(VertexBufferRHI, IntermediateBuffer);

		if (ShaderResourceView)
		{
			//Batcher.QueueUpdateRequest(ShaderResourceView, VertexBufferRHI);
		}
	}

	void InitRHIFromExisting(const FBufferRHIRef& InVertexBufferRHI)
	{
		InitResource(GetImmediateCommandList());
		check(InVertexBufferRHI);

		VertexBufferRHI = InVertexBufferRHI;
		if (VertexBufferRHI.IsValid() && RHISupportsManualVertexFetch(GMaxRHIShaderPlatform))
		{
			ShaderResourceView = GetImmediateCommandList().CreateShaderResourceView(FShaderResourceViewInitializer(VertexBufferRHI, GetElementFormat()));
		}
	}
};


class FRuntimeMeshPositionVertexBuffer : public FRuntimeMeshVertexBuffer
{
public:
	FRuntimeMeshPositionVertexBuffer(bool bInIsDynamicBuffer)
		: FRuntimeMeshVertexBuffer(bInIsDynamicBuffer, sizeof(FVector3f))
	{

	}

	virtual FString GetFriendlyName() const override { return TEXT("FRuntimeMeshPositionVertexBuffer"); }

	virtual int32 GetElementDatumSize() const override { return 4; }
	virtual EPixelFormat GetElementFormat() const override { return PF_R32_FLOAT; }

	virtual void Bind(FLocalVertexFactory::FDataType& DataType) override 
	{
		DataType.PositionComponent = FVertexStreamComponent(this, 0, 12, VET_Float3);
		DataType.PositionComponentSRV = ShaderResourceView;
	}

	//template <uint32 MaxNumUpdates>
	void UpdateRHIFromExisting(FRHIBuffer* IntermediateBuffer, int32 NumElements, FRHIResourceReplaceBatcher& Batcher)
	{
		if (VertexBufferRHI && IntermediateBuffer)
		{
			VertexSize = sizeof(FVector3f);
			NumVertices = NumElements;

			FRuntimeMeshVertexBuffer::UpdateRHIFromExisting(IntermediateBuffer, Batcher);
		}
	}

	void InitRHIFromExisting(const FBufferRHIRef& InVertexBufferRHI, int32 NumElements)
	{
		VertexSize = sizeof(FVector3f);
		NumVertices = NumElements;

		FRuntimeMeshVertexBuffer::InitRHIFromExisting(InVertexBufferRHI);
	}
};

class FRuntimeMeshTangentsVertexBuffer : public FRuntimeMeshVertexBuffer
{
	static constexpr int32 CalculateStride(bool bShouldUseHighPrecision)
	{
		return (bShouldUseHighPrecision ? sizeof(FPackedRGBA16N) : sizeof(FPackedNormal)) * 2;
	}

private:

	/** Whether this tangent buffer is using high precision tangents */
	bool bUseHighPrecision;

public:
	FRuntimeMeshTangentsVertexBuffer(bool bInIsDynamicBuffer)
		: FRuntimeMeshVertexBuffer(bInIsDynamicBuffer, CalculateStride(false))
		, bUseHighPrecision(false)
	{

	}

	virtual FString GetFriendlyName() const override { return TEXT("FRuntimeMeshTangentsVertexBuffer"); }

	virtual int32 GetElementDatumSize() const override { return bUseHighPrecision? sizeof(FPackedRGBA16N) : sizeof(FPackedNormal); }
	virtual EPixelFormat GetElementFormat() const override { return bUseHighPrecision ? PF_R16G16B16A16_SNORM : PF_R8G8B8A8_SNORM; }

	virtual void Bind(FLocalVertexFactory::FDataType& DataType) override
	{
		uint32 TangentSizeInBytes = 0;
		uint32 TangentXOffset = 0;
		uint32 TangentZOffset = 0;
		EVertexElementType TangentElementType = VET_None;

		if (bUseHighPrecision)
		{
			TangentElementType = VET_UShort4N;
			TangentSizeInBytes = sizeof(FPackedRGBA16N) * 2;
			TangentXOffset = 0;
			TangentZOffset = sizeof(FPackedRGBA16N);
		}
		else
		{
			TangentElementType = VET_PackedNormal;
			TangentSizeInBytes = sizeof(FPackedNormal) * 2;
			TangentXOffset = 0;
			TangentZOffset = sizeof(FPackedNormal);
		}

		DataType.TangentBasisComponents[0] = FVertexStreamComponent(this, TangentXOffset, TangentSizeInBytes, TangentElementType, EVertexStreamUsage::ManualFetch);
		DataType.TangentBasisComponents[1] = FVertexStreamComponent(this, TangentZOffset, TangentSizeInBytes, TangentElementType, EVertexStreamUsage::ManualFetch);
		DataType.TangentsSRV = ShaderResourceView;
	}

	//template <uint32 MaxNumUpdates>
	void UpdateRHIFromExisting(FRHIBuffer* IntermediateBuffer, int32 NumElements, bool bShouldUseHighPrecision, FRHIResourceReplaceBatcher& Batcher)
	{
		if (VertexBufferRHI && IntermediateBuffer)
		{
			bUseHighPrecision = bShouldUseHighPrecision;
			VertexSize = CalculateStride(bShouldUseHighPrecision);
			NumVertices = NumElements;

			FRuntimeMeshVertexBuffer::UpdateRHIFromExisting(IntermediateBuffer, Batcher);
		}
	}

	void InitRHIFromExisting(const FBufferRHIRef& InVertexBufferRHI, int32 NumElements, bool bShouldUseHighPrecision)
	{
		bUseHighPrecision = bShouldUseHighPrecision;
		VertexSize = CalculateStride(bShouldUseHighPrecision);
		NumVertices = NumElements;

		FRuntimeMeshVertexBuffer::InitRHIFromExisting(InVertexBufferRHI);
	}

};

class FRuntimeMeshTexCoordsVertexBuffer : public FRuntimeMeshVertexBuffer
{
	static constexpr int32 CalculateStride(bool bShouldUseHighPrecision, int32 InNumUVs)
	{
		return (bShouldUseHighPrecision ? sizeof(FVector2f) : sizeof(FVector2DHalf)) * InNumUVs;
	}

private:
	/** Whether this uv buffer is using high precision uvs */
	bool bUseHighPrecision;

	/** Num UV's in use */
	int32 NumUVs;

public:
	FRuntimeMeshTexCoordsVertexBuffer(bool bInIsDynamicBuffer)
		: FRuntimeMeshVertexBuffer(bInIsDynamicBuffer, CalculateStride(false, 1))
		, bUseHighPrecision(false)
		, NumUVs(1)
	{

	}

	virtual FString GetFriendlyName() const override { return TEXT("FRuntimeMeshUVsVertexBuffer"); }

	virtual int32 GetElementDatumSize() const override { return bUseHighPrecision ? sizeof(FVector2f) : sizeof(FVector2DHalf); }
	virtual EPixelFormat GetElementFormat() const override { return bUseHighPrecision ? PF_G32R32F : PF_G16R16F; }

	virtual void Bind(FLocalVertexFactory::FDataType& DataType) override
	{ 
		DataType.TextureCoordinates.Empty();
		DataType.NumTexCoords = NumUVs;


		EVertexElementType UVDoubleWideVertexElementType = VET_None;
		EVertexElementType UVVertexElementType = VET_None;
		uint32 UVSizeInBytes = 0;
		if (bUseHighPrecision)
		{
			UVSizeInBytes = sizeof(FVector2f);
			UVDoubleWideVertexElementType = VET_Float4;
			UVVertexElementType = VET_Float2;
		}
		else
		{
			UVSizeInBytes = sizeof(FVector2DHalf);
			UVDoubleWideVertexElementType = VET_Half4;
			UVVertexElementType = VET_Half2;
		}

		uint32 UVStride = UVSizeInBytes * NumUVs;

		int32 UVIndex;
		for (UVIndex = 0; UVIndex < NumUVs - 1; UVIndex += 2)
		{
			DataType.TextureCoordinates.Add(FVertexStreamComponent(this, UVSizeInBytes * UVIndex, UVStride, UVDoubleWideVertexElementType, EVertexStreamUsage::ManualFetch));
		}

		// possible last UV channel if we have an odd number
		if (UVIndex < NumUVs)
		{
			DataType.TextureCoordinates.Add(FVertexStreamComponent(this, UVSizeInBytes * UVIndex, UVStride, UVVertexElementType, EVertexStreamUsage::ManualFetch));
		}

		DataType.TextureCoordinatesSRV = ShaderResourceView;
	}

	//template <uint32 MaxNumUpdates>
	void UpdateRHIFromExisting(FRHIBuffer* IntermediateBuffer, int32 NumElements, bool bShouldUseHighPrecision, int32 NumChannels, FRHIResourceReplaceBatcher& Batcher)
	{
		if (VertexBufferRHI && IntermediateBuffer)
		{
			bUseHighPrecision = bShouldUseHighPrecision;
			NumUVs = NumChannels;
			VertexSize = CalculateStride(bShouldUseHighPrecision, NumChannels);
			NumVertices = NumElements;

			FRuntimeMeshVertexBuffer::UpdateRHIFromExisting(IntermediateBuffer, Batcher);
		}
	}

	void InitRHIFromExisting(const FBufferRHIRef& InVertexBufferRHI, int32 NumElements, bool bShouldUseHighPrecision, int32 NumChannels)
	{
		bUseHighPrecision = bShouldUseHighPrecision;
		NumUVs = NumChannels;
		VertexSize = CalculateStride(bShouldUseHighPrecision, NumChannels);
		NumVertices = NumElements;

		FRuntimeMeshVertexBuffer::InitRHIFromExisting(InVertexBufferRHI);
	}

};

class FRuntimeMeshColorVertexBuffer : public FRuntimeMeshVertexBuffer
{
public:
	FRuntimeMeshColorVertexBuffer(bool bInIsDynamicBuffer)
		: FRuntimeMeshVertexBuffer(bInIsDynamicBuffer, sizeof(FColor))
	{

	}

	virtual FString GetFriendlyName() const override { return TEXT("FRuntimeMeshColorVertexBuffer"); }

	virtual int32 GetElementDatumSize() const override { return sizeof(FColor); }
	virtual EPixelFormat GetElementFormat() const override { return PF_R8G8B8A8; }

	virtual void Bind(FLocalVertexFactory::FDataType& DataType) override
	{
		DataType.ColorComponent = FVertexStreamComponent(this, 0, 4, EVertexElementType::VET_Color, EVertexStreamUsage::ManualFetch);
		DataType.ColorComponentsSRV = ShaderResourceView;
	}

	//template <uint32 MaxNumUpdates>
	void UpdateRHIFromExisting(FRHIBuffer* IntermediateBuffer, int32 NumElements, FRHIResourceReplaceBatcher& Batcher)
	{
		if (VertexBufferRHI && IntermediateBuffer)
		{
			VertexSize = sizeof(FColor);
			NumVertices = NumElements;

			FRuntimeMeshVertexBuffer::UpdateRHIFromExisting(IntermediateBuffer, Batcher);
		}
	}

	void InitRHIFromExisting(const FBufferRHIRef& InVertexBufferRHI, int32 NumElements)
	{
		VertexSize = sizeof(FColor);
		NumVertices = NumElements;

		FRuntimeMeshVertexBuffer::InitRHIFromExisting(InVertexBufferRHI);
	}

};

/** Index Buffer */
class FRuntimeMeshIndexBuffer : public FIndexBuffer
{
	static constexpr int32 CalculateStride(bool bShouldUseHighPrecision)
	{
		return bShouldUseHighPrecision ? sizeof(int32) : sizeof(uint16);
	}

private:
	/** Should this buffer by flagged as dynamic */
	bool bIsDynamicBuffer;

	/* The size of a single index*/
	int32 IndexSize;

	/* The number of indices this buffer is currently allocated to hold */
	int32 NumIndices;

public:

	FRuntimeMeshIndexBuffer(bool bInIsDynamicBuffer);

	virtual FString GetFriendlyName() const override { return TEXT("FRuntimeMeshIndexBuffer"); }

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;

	/* Get the size of the index buffer */
	int32 Num() const { return NumIndices; }

	/** Gets the full allocated size of the buffer (Equal to IndexSize * NumIndices) */
	int32 GetBufferSize() const { return NumIndices * IndexSize; }

	template<bool bIsInRenderThread>
	static FBufferRHIRef CreateRHIBuffer(FRuntimeMeshBufferUpdateData& InStream, bool bDynamicBuffer)
	{
		const uint32 SizeInBytes = InStream.GetResourceDataSize();

		const int32 NumElements = InStream.GetNumElements();
		const int32 Stride = InStream.GetStride();
		const bool bIsNullStream = !(NumElements > 0 && Stride > 0);

		if (Stride > 0 && NumElements > 0)
		{
			if ((uint64)Stride * (uint64)NumElements != (uint64)SizeInBytes)
			{
				UE_LOG(RuntimeMeshLog, Warning, TEXT("CreateRHIBuffer(Index): size mismatch: elements=%d stride=%d computedSize=%llu actualSize=%u"), NumElements, Stride, (unsigned long long)Stride * (unsigned long long)NumElements, SizeInBytes);
			}
		}
		else
		{
			UE_LOG(RuntimeMeshLog, Verbose, TEXT("CreateRHIBuffer(Index): creating NULL buffer (elements=%d stride=%d size=%u dynamic=%d)"), NumElements, Stride, SizeInBytes, bDynamicBuffer);
		}

		FRHIBufferCreateDesc BufferDesc;

		if (!bIsNullStream)
		{
			// Create an index buffer for index streams
			BufferDesc = FRHIBufferCreateDesc::CreateIndex(TEXT("RuntimeMeshComponent"))
				.SetSize(SizeInBytes)
				.SetStride(Stride)
				.SetUsage((bDynamicBuffer ? BUF_Dynamic : BUF_Static) | BUF_IndexBuffer)
				.SetInitialState(ERHIAccess::VertexOrIndexBuffer)
				.SetInitActionResourceArray(&InStream);
		}
		else
		{
			// Ensure null buffers still have a sane initial state to avoid RHI validation issues
			BufferDesc = FRHIBufferCreateDesc::CreateNull(TEXT("RuntimeMeshComponent")).SetInitialState(ERHIAccess::VertexOrIndexBuffer);
		}

		UE_LOG(RuntimeMeshLog, Verbose, TEXT("CreateRHIBuffer(Index): creating buffer (elements=%d stride=%d size=%u dynamic=%d isNull=%d)"), NumElements, Stride, SizeInBytes, bDynamicBuffer, (int)bIsNullStream);

		FBufferRHIRef NewBuffer = GetImmediateCommandList().CreateBuffer(BufferDesc);

		if (NewBuffer.IsValid())
		{
			UE_LOG(RuntimeMeshLog, Verbose, TEXT("CreateRHIBuffer(Index): buffer created. Usage=%u"), (uint32)NewBuffer->GetUsage());
		}
		else
		{
			UE_LOG(RuntimeMeshLog, Warning, TEXT("CreateRHIBuffer(Index): CreateBuffer returned null!"));
		}

		return NewBuffer;
	}


	//template <uint32 MaxNumUpdates>
	void UpdateRHIFromExisting(FRHIBuffer* IntermediateBuffer, int32 NumElements, bool bShouldUseHighPrecision, FRHIResourceReplaceBatcher& Batcher)
	{
		if (IndexBufferRHI && IntermediateBuffer)
		{
			IndexSize = CalculateStride(bShouldUseHighPrecision);
			NumIndices = NumElements;

			Batcher.EnqueueReplace(IndexBufferRHI, IntermediateBuffer);
		}
	}

	void InitRHIFromExisting(const FBufferRHIRef& InIndexBufferRHI, int32 NumElements, bool bShouldUseHighPrecision)
	{
		InitResource(GetImmediateCommandList());

		IndexSize = CalculateStride(bShouldUseHighPrecision);
		NumIndices = NumElements;

		if (InIndexBufferRHI)
		{
			IndexBufferRHI = InIndexBufferRHI;
		}
		else
		{
			FRHIBufferCreateDesc IndexBufferDesc = FRHIBufferCreateDesc::CreateIndex(TEXT("RuntimeMeshComponent"))
				.SetStride(IndexSize)
				.SetSize(NumIndices * IndexSize)
				.SetUsage((bIsDynamicBuffer ? BUF_Dynamic : BUF_Static) | BUF_IndexBuffer);
			IndexBufferRHI = GetImmediateCommandList().CreateBuffer(IndexBufferDesc);
		}
	}

};

/** Vertex Factory */
class FRuntimeMeshVertexFactory : public FLocalVertexFactory
{
public:

	FRuntimeMeshVertexFactory(ERHIFeatureLevel::Type InFeatureLevel);

	/** Init function that can be called on any thread, and will do the right thing (enqueue command if called on main thread) */
	void Init(FLocalVertexFactory::FDataType VertexStructure);

private:

};

/** Deleter function for TSharedPtrs that only allows the object to be destructed on the render thread. */
template<typename Type>
struct FRuntimeMeshRenderThreadDeleter
{
	void operator()(Type* Object) const
	{
		// This is a custom deleter to make sure the runtime mesh proxy is only ever deleted on the rendering thread.
		if (IsInRenderingThread())
		{
			delete Object;
		}
		else
		{
			ENQUEUE_RENDER_COMMAND(FRuntimeMeshProxyDeleterCommand)(
				[Object](FRHICommandListImmediate& RHICmdList)
				{
					delete static_cast<Type*>(Object);
				}
			);
		}
	}
};

#define RMC_LOG_VERBOSE(Format, ...) \
	UE_LOG(RuntimeMeshLog, Verbose, TEXT("[Thread:%d]: " Format), FPlatformTLS::GetCurrentThreadId(), ##__VA_ARGS__);


template<bool bIsInRenderThread>
void FRuntimeMeshSectionUpdateData::CreateRHIBuffers(bool bShouldUseDynamicBuffers)
{
	if (!bBuffersCreated)
	{
		PositionsBuffer = FRuntimeMeshVertexBuffer::CreateRHIBuffer<bIsInRenderThread>(Positions, bShouldUseDynamicBuffers);
		TangentsBuffer = FRuntimeMeshVertexBuffer::CreateRHIBuffer<bIsInRenderThread>(Tangents, bShouldUseDynamicBuffers);
		TexCoordsBuffer = FRuntimeMeshVertexBuffer::CreateRHIBuffer<bIsInRenderThread>(TexCoords, bShouldUseDynamicBuffers);
		ColorsBuffer = FRuntimeMeshVertexBuffer::CreateRHIBuffer<bIsInRenderThread>(Colors, bShouldUseDynamicBuffers);

		TrianglesBuffer = FRuntimeMeshIndexBuffer::CreateRHIBuffer<bIsInRenderThread>(Triangles, bShouldUseDynamicBuffers);
		AdjacencyTrianglesBuffer = FRuntimeMeshIndexBuffer::CreateRHIBuffer<bIsInRenderThread>(AdjacencyTriangles, bShouldUseDynamicBuffers);

		bBuffersCreated = true;
	}
}


#undef RMC_LOG_VERBOSE
