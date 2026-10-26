#include "D3DPipeline.h"
#include "D3DAccelerationStructure.h"
#include "D3DProgram.h"
#include <set>

bool D3DPipeline::createRayTracing(D3DContext &context,
    D3DAccelerationStructure *accelerationStructure)
{
    if (std::exchange(mCreated, true))
        return (mRayTracingStateObject && mRootSignature);

    mAccelerationStructure = accelerationStructure;
    if (!createRootSignatureRayTracing(context))
        return false;
    createDescriptorHeap(context);

    if (!mProgram.rayTracing().isEmpty())
        return createRayTracingExplicit(context);

    auto device = ComPtr<ID3D12Device5>();
    if (FAILED(context.device.QueryInterface(IID_PPV_ARGS(&device)))) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }

    const auto rayGeneration =
        mProgram.getShader(Shader::ShaderType::RayGeneration);
    const auto rayMiss = mProgram.getShader(Shader::ShaderType::RayMiss);
    const auto closestHit =
        mProgram.getShader(Shader::ShaderType::RayClosestHit);
    const auto anyHit = mProgram.getShader(Shader::ShaderType::RayAnyHit);
    const auto intersection =
        mProgram.getShader(Shader::ShaderType::RayIntersection);
    if (!rayGeneration || !rayMiss) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }

    auto stateObject =
        CD3DX12_STATE_OBJECT_DESC(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);
    for (const auto &shader : mProgram.shaders()) {
        if (!isRayTracingShaderType(shader.type()))
            continue;

        const auto bytecode = D3D12_SHADER_BYTECODE{
            shader.binary()->GetBufferPointer(),
            shader.binary()->GetBufferSize(),
        };
        auto library =
            stateObject.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        library->SetDXILLibrary(&bytecode);
        library->DefineExport(shader.entryPoint().toStdWString().c_str());
    }

    constexpr auto hitGroupName = L"HitGroup";
    if (closestHit || anyHit || intersection) {
        auto hitGroup =
            stateObject.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        hitGroup->SetHitGroupExport(hitGroupName);
        hitGroup->SetHitGroupType(intersection
                ? D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE
                : D3D12_HIT_GROUP_TYPE_TRIANGLES);
        if (closestHit)
            hitGroup->SetClosestHitShaderImport(
                closestHit->entryPoint().toStdWString().c_str());
        if (anyHit)
            hitGroup->SetAnyHitShaderImport(
                anyHit->entryPoint().toStdWString().c_str());
        if (intersection)
            hitGroup->SetIntersectionShaderImport(
                intersection->entryPoint().toStdWString().c_str());
    }

    auto shaderConfig =
        stateObject
            .CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    shaderConfig->Config(128, D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES);

    auto rootSignature =
        stateObject.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    rootSignature->SetRootSignature(mRootSignature.Get());

    auto pipelineConfig =
        stateObject
            .CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    pipelineConfig->Config(1);

    if (FAILED(device->CreateStateObject(stateObject,
            IID_PPV_ARGS(&mRayTracingStateObject)))) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }

    auto properties = ComPtr<ID3D12StateObjectProperties>();
    if (FAILED(mRayTracingStateObject.As(&properties))) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }

    constexpr auto identifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    constexpr auto tableAlignment =
        D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
    constexpr auto rayGenerationOffset = UINT64{ 0 };
    constexpr auto missOffset = UINT64{ tableAlignment };
    constexpr auto hitGroupOffset = UINT64{ tableAlignment * 2 };
    constexpr auto bufferSize = UINT64{ tableAlignment * 3 };

    const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    if (FAILED(context.device.CreateCommittedResource(&heapProperties,
            D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&mShaderBindingTable)))) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }

    auto mappedData = std::add_pointer_t<void>{ };
    if (FAILED(mShaderBindingTable->Map(0, nullptr, &mappedData))) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }
    std::memset(mappedData, 0, static_cast<size_t>(bufferSize));
    const auto copyIdentifier = [&](UINT64 offset, const wchar_t *name) {
        if (const auto identifier = properties->GetShaderIdentifier(name)) {
            std::memcpy(static_cast<std::byte *>(mappedData) + offset,
                identifier, identifierSize);
            return true;
        }
        return false;
    };
    const auto identifiersValid =
        copyIdentifier(rayGenerationOffset,
            rayGeneration->entryPoint().toStdWString().c_str())
        && copyIdentifier(missOffset,
            rayMiss->entryPoint().toStdWString().c_str())
        && (!(closestHit || anyHit || intersection)
            || copyIdentifier(hitGroupOffset, hitGroupName));
    mShaderBindingTable->Unmap(0, nullptr);
    if (!identifiersValid) {
        mMessages.insert(mItemId, MessageType::CreatingPipelineFailed);
        return false;
    }

    const auto address = mShaderBindingTable->GetGPUVirtualAddress();
    mRayGenerationShaderRecord = {
        .StartAddress = address + rayGenerationOffset,
        .SizeInBytes = identifierSize,
    };
    mMissShaderTable = {
        .StartAddress = address + missOffset,
        .SizeInBytes = identifierSize,
        .StrideInBytes = identifierSize,
    };
    if (closestHit || anyHit || intersection)
        mHitGroupTable = {
            .StartAddress = address + hitGroupOffset,
            .SizeInBytes = identifierSize,
            .StrideInBytes = identifierSize,
        };
    return true;
}

bool D3DPipeline::createRayTracingExplicit(D3DContext &context)
{
    const auto &config = mProgram.rayTracing();
    const auto fail = [&](const QString &text) {
        mMessages.insert(mItemId, MessageType::ScriptError, "DXR records: " + text);
        return false;
    };
    if (config.contains("_invalidType"))
        return fail("rayTracing must be a JSON object");
    if (config.contains("localRoots"))
        return fail("Local roots belong to the later D2 contribution");
    for (const auto key : { "hitGroups", "hitRecords", "missRecords" })
        if (config.value(key).typeId() != QMetaType::QVariantList)
            return fail(QStringLiteral("%1 must be an array").arg(key));

    auto shaders = std::map<std::wstring, const D3DShader *>();
    for (const auto &shader : mProgram.shaders()) {
        if (!isRayTracingShaderType(shader.type()))
            return fail("Explicit DXR programs may contain only ray stages and includables");
        if (!shaders.emplace(shader.entryPoint().toStdWString(), &shader).second)
            return fail("Duplicate export: " + shader.entryPoint());
    }
    const auto hasStage = [&](const std::wstring &name, Shader::ShaderType type) {
        const auto it = shaders.find(name);
        return it != shaders.end() && it->second->type() == type;
    };
    const auto raygen = config.value("rayGenerationRecord").toMap();
    if (raygen.contains("arguments"))
        return fail("Local arguments belong to D2");
    const auto raygenName = raygen.value("export").toString().toStdWString();
    if (!hasStage(raygenName, Shader::ShaderType::RayGeneration))
        return fail("rayGenerationRecord must select a raygeneration export");

    struct Group {
        std::wstring name, closestHit, anyHit, intersection;
        bool procedural;
    };
    auto groups = std::vector<Group>();
    auto groupNames = std::set<std::wstring>();
    for (const auto &value : config.value("hitGroups").toList()) {
        const auto item = value.toMap();
        const auto type = item.value("type").toString();
        if (type != "Triangles" && type != "Procedural")
            return fail("Hit-group type must be Triangles or Procedural");
        if (!item.value("localRoot").toString().isEmpty())
            return fail("Local roots belong to D2");
        auto group = Group{
            item.value("name").toString().toStdWString(),
            item.value("closestHit").toString().toStdWString(),
            item.value("anyHit").toString().toStdWString(),
            item.value("intersection").toString().toStdWString(),
            type == "Procedural",
        };
        if (group.name.empty() || shaders.contains(group.name)
            || !groupNames.insert(group.name).second)
            return fail("Empty, duplicate or conflicting hit-group name");
        if ((!group.closestHit.empty() && !hasStage(group.closestHit, Shader::ShaderType::RayClosestHit))
            || (!group.anyHit.empty() && !hasStage(group.anyHit, Shader::ShaderType::RayAnyHit))
            || (group.procedural && !hasStage(group.intersection, Shader::ShaderType::RayIntersection))
            || (!group.procedural && !group.intersection.empty()))
            return fail("Hit-group imports do not match their required shader stages");
        groups.push_back(std::move(group));
    }

    const auto readRecords = [&](const char *key, bool hitTable,
                                 std::vector<std::wstring> &names) {
        for (const auto &value : config.value(key).toList()) {
            const auto record = value.toMap();
            if (record.contains("arguments"))
                return fail("Local arguments belong to D2");
            const auto name = record.value("export").toString().toStdWString();
            if (hitTable ? !groupNames.contains(name)
                         : !hasStage(name, Shader::ShaderType::RayMiss))
                return fail(QStringLiteral("Unknown or wrong-stage export in %1").arg(key));
            names.push_back(name);
        }
        return true;
    };
    auto missNames = std::vector<std::wstring>();
    auto hitNames = std::vector<std::wstring>();
    if (!readRecords("missRecords", false, missNames)
        || !readRecords("hitRecords", true, hitNames))
        return false;
    if (missNames.empty())
        return fail("At least one Miss record is required by this host interface");

    auto device = ComPtr<ID3D12Device5>();
    if (FAILED(context.device.QueryInterface(IID_PPV_ARGS(&device))))
        return fail("ID3D12Device5 is unavailable");
    auto stateObject = CD3DX12_STATE_OBJECT_DESC(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);
    for (const auto &[name, shader] : shaders) {
        const auto bytecode = D3D12_SHADER_BYTECODE{
            shader->binary()->GetBufferPointer(), shader->binary()->GetBufferSize(),
        };
        auto library = stateObject.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        library->SetDXILLibrary(&bytecode);
        library->DefineExport(name.c_str());
    }
    for (const auto &group : groups) {
        auto desc = stateObject.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        desc->SetHitGroupExport(group.name.c_str());
        desc->SetHitGroupType(group.procedural ? D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE
                                             : D3D12_HIT_GROUP_TYPE_TRIANGLES);
        if (!group.closestHit.empty()) desc->SetClosestHitShaderImport(group.closestHit.c_str());
        if (!group.anyHit.empty()) desc->SetAnyHitShaderImport(group.anyHit.c_str());
        if (!group.intersection.empty()) desc->SetIntersectionShaderImport(group.intersection.c_str());
    }
    auto shaderConfig = stateObject.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    shaderConfig->Config(128, D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES);
    auto root = stateObject.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    root->SetRootSignature(mRootSignature.Get());
    auto pipeline = stateObject.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    pipeline->Config(1);
    if (FAILED(device->CreateStateObject(stateObject, IID_PPV_ARGS(&mRayTracingStateObject))))
        return fail("CreateStateObject failed for configured exports/groups");
    auto properties = ComPtr<ID3D12StateObjectProperties>();
    if (FAILED(mRayTracingStateObject.As(&properties)))
        return fail("State-object properties unavailable");

    constexpr auto stride = UINT64{ D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES };
    const auto alignTable = [](UINT64 size) {
        constexpr auto alignment = UINT64{ D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT };
        return (size + alignment - 1) & ~(alignment - 1);
    };
    const auto missOffset = alignTable(stride);
    const auto hitOffset = alignTable(missOffset + missNames.size() * stride);
    const auto bufferSize = alignTable(hitOffset + hitNames.size() * stride);
    const auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const auto buffer = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    if (FAILED(context.device.CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
            &buffer, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&mShaderBindingTable))))
        return fail("Shader-table allocation failed");
    auto data = std::add_pointer_t<void>{};
    if (FAILED(mShaderBindingTable->Map(0, nullptr, &data)))
        return fail("Shader-table mapping failed");
    std::memset(data, 0, static_cast<size_t>(bufferSize));
    const auto info = qEnvironmentVariableIsSet("GPUPAD_DXR_SBT_INFO");
    const auto copy = [&](UINT64 offset, const std::wstring &name) {
        const auto identifier = properties->GetShaderIdentifier(name.c_str());
        if (!identifier) return false;
        std::memcpy(static_cast<std::byte *>(data) + offset, identifier, stride);
        if (info)
            mMessages.insert(mItemId, MessageType::DeviceInfo,
                QStringLiteral("DXR SBT record: offset=%1 export=%2")
                    .arg(offset).arg(QString::fromStdWString(name)));
        return true;
    };
    auto valid = copy(0, raygenName);
    for (auto i = size_t{}; i < missNames.size(); ++i)
        valid &= copy(missOffset + i * stride, missNames[i]);
    for (auto i = size_t{}; i < hitNames.size(); ++i)
        valid &= copy(hitOffset + i * stride, hitNames[i]);
    mShaderBindingTable->Unmap(0, nullptr);
    if (!valid) return fail("Configured export has no shader identifier");

    const auto address = mShaderBindingTable->GetGPUVirtualAddress();
    mRayGenerationShaderRecord = { address, stride };
    mMissShaderTable = { address + missOffset, missNames.size() * stride, stride };
    mHitGroupTable = hitNames.empty() ? D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE{}
        : D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE{
              address + hitOffset, hitNames.size() * stride, stride };
    if (info)
        mMessages.insert(mItemId, MessageType::DeviceInfo,
            QStringLiteral("DXR SBT: missRecords=%1 hitRecords=%2 stride=%3 missOffset=%4 hitOffset=%5 bytes=%6")
                .arg(missNames.size()).arg(hitNames.size()).arg(stride)
                .arg(missOffset).arg(hitOffset).arg(bufferSize));
    return true;
}

bool D3DPipeline::createRootSignatureRayTracing(D3DContext &context)
{
    auto descriptorTableDescs = std::vector<DescriptorTableDesc>();
    auto staticSamplers = std::vector<CD3DX12_STATIC_SAMPLER_DESC>();
    auto ranges = std::vector<CD3DX12_DESCRIPTOR_RANGE>();
    auto resources =
        std::set<std::tuple<D3D12_DESCRIPTOR_RANGE_TYPE, UINT, UINT>>();
    auto samplers = std::set<std::pair<UINT, UINT>>();
    auto descriptorHeapOffset = UINT{ };
    for (const auto &shader : mProgram.shaders()) {
        if (!shader.reflection())
            continue;
        for (const auto &binding : shader.reflection().descriptorBindings()) {
            const auto bindDesc = getD3DBindingDesc(binding);
            if (bindDesc.Type == D3D_SIT_SAMPLER) {
                if (samplers.emplace(bindDesc.Space, bindDesc.BindPoint).second)
                    addStaticSampler(bindDesc, &staticSamplers);
                continue;
            }

            // TODO: fix non uniform indexing
            if (!bindDesc.BindCount) {
                mMessages.insert(mItemId, MessageType::NotImplemented,
                    "Non-Uniform Indexing");
                return false;
            }

            const auto rangeType = getRangeType(bindDesc.Type);
            if (!resources
                    .emplace(rangeType, bindDesc.Space, bindDesc.BindPoint)
                    .second)
                continue;
            ranges.emplace_back().Init(rangeType, bindDesc.BindCount,
                bindDesc.BindPoint, bindDesc.Space);
            mDescriptorHeapEntries.push_back({
                .visibility = D3D12_SHADER_VISIBILITY_ALL,
                .rangeType = rangeType,
                .space = bindDesc.Space,
                .bindPoint = bindDesc.BindPoint,
                .offset = descriptorHeapOffset,
                .count = bindDesc.BindCount,
            });
            descriptorHeapOffset += bindDesc.BindCount;
        }
    }

    if (!ranges.empty()) {
        mDescriptorTableEntries.push_back(descriptorHeapOffset);
        descriptorTableDescs.push_back({
            .visibility = D3D12_SHADER_VISIBILITY_ALL,
            .ranges = std::move(ranges),
        });
    }
    return createRootSignatureFromTables(context,
        std::move(descriptorTableDescs), std::move(staticSamplers));
}

bool D3DPipeline::bindRayTracing(D3DContext &context,
    ScriptEngine &scriptEngine)
{
    auto commandList = ComPtr<ID3D12GraphicsCommandList4>();
    if (FAILED(context.graphicsCommandList.As(&commandList))) {
        mMessages.insert(mItemId, MessageType::CallFailed);
        return false;
    }

    if (!setDescriptors(context, scriptEngine))
        return false;

    commandList->SetPipelineState1(mRayTracingStateObject.Get());
    commandList->SetComputeRootSignature(mRootSignature.Get());

    if (mDescriptorHeap) {
        auto descriptorHeap = mDescriptorHeap.Get();
        commandList->SetDescriptorHeaps(1, &descriptorHeap);

        auto descriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE{
            mDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
        };
        for (auto i = 0u; i < mDescriptorTableEntries.size(); ++i) {
            commandList->SetComputeRootDescriptorTable(i, descriptor);
            descriptor.Offset(mDescriptorTableEntries[i],
                context.descriptorSize);
        }
    }
    return true;
}

void D3DPipeline::dispatchRays(D3DContext &context, UINT width, UINT height,
    UINT depth)
{
    auto commandList = ComPtr<ID3D12GraphicsCommandList4>();
    if (FAILED(context.graphicsCommandList.As(&commandList))) {
        mMessages.insert(mItemId, MessageType::CallFailed);
        return;
    }

    const auto desc = D3D12_DISPATCH_RAYS_DESC{
        .RayGenerationShaderRecord = mRayGenerationShaderRecord,
        .MissShaderTable = mMissShaderTable,
        .HitGroupTable = mHitGroupTable,
        .Width = width,
        .Height = height,
        .Depth = depth,
    };
    commandList->DispatchRays(&desc);
}
