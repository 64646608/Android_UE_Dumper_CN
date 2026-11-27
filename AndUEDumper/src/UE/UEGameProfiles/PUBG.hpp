#pragma once

#include "../UEGameProfile.hpp"
using namespace UEMemory;

class PUBGProfile : public IGameProfile
{
public:
    PUBGProfile() = default;

    // Current private-server / 1.39 layout verified from the running ARM64 build.
    static constexpr uintptr_t kGNamesRva        = 0x176E6650;
    static constexpr uintptr_t kGUObjectArrayRva = 0x17D46E08;
    static constexpr uintptr_t kObjObjectsRva    = 0x17D46E18;
    static constexpr uintptr_t kPhysxRva         = 0x181A36B0;

    bool ArchSupprted() const override
    {
        auto e_machine = GetUnrealELF().header().e_machine;
        return e_machine == EM_AARCH64;
    }

    std::string GetAppName() const override
    {
        return "PUBGM private-server current";
    }

    std::vector<std::string> GetAppIDs() const override
    {
        return {"com.tencent.tmgp.pubgmhd"};
    }

    bool isUsingCasePreservingName() const override
    {
        return false;
    }

    bool IsUsingFNamePool() const override
    {
        // Current verified name source is the legacy chunked GNames table.
        return false;
    }

    bool isUsingOutlineNumberName() const override
    {
        return false;
    }

    uintptr_t GetGUObjectArrayPtr() const override
    {
        const auto base = GetUEVars()->GetBaseAddress();
        const auto ptr = base + kGUObjectArrayRva;
        printf("[PUBGM-QA] GUObjectArray: base+0x%lX = 0x%lX\n",
               (unsigned long)kGUObjectArrayRva, (unsigned long)ptr);
        return ptr;
    }

    uintptr_t GetNamesPtr() const override
    {
        const auto base = GetUEVars()->GetBaseAddress();
        const auto ptr = base + kGNamesRva;
        printf("[PUBGM-QA] GNames: base+0x%lX = 0x%lX\n",
               (unsigned long)kGNamesRva, (unsigned long)ptr);
        printf("[PUBGM-QA] Expected ObjObjects: base+0x%lX\n",
               (unsigned long)kObjObjectsRva);
        printf("[PUBGM-QA] UObject.NamePrivate=0x48 FNameEntry.Name=0x0C\n");
        return ptr;
    }

    uintptr_t GetFrameCount() const override
    {
        // Old signature is version-specific. Do not report a false match.
        return 0;
    }

    uintptr_t GetMatrix() const override
    {
        // Current project derives the view from CameraManager; the old profile
        // signature is stale. Matrix is optional for SDK class dumping.
        return 0;
    }

    uintptr_t GetPhysx() const override
    {
        // Verified global slot from current runtime audit.
        return GetUEVars()->GetBaseAddress() + kPhysxRva;
    }

    UE_Offsets *GetOffsets() const override
    {
        // Keep the project's legacy-name/TUObjectArray baseline, but override
        // only fields already verified on this current build. Do not guess
        // ClassPrivate/OuterPrivate/UStruct offsets here; the next device run
        // will tell us exactly where the dump advances or stops.
        static UE_Offsets offsets =
            UE_DefaultOffsets::UE4_18_19(isUsingCasePreservingName());

        static bool once = false;
        if (!once)
        {
            once = true;
            offsets.FNameEntry.Index = sizeof(void *);
            offsets.FNameEntry.Name = 0x0C;
            offsets.TUObjectArray.NumElements = 0x38;
            offsets.UObject.NamePrivate = 0x48;
        }

        return &offsets;
    }
};
