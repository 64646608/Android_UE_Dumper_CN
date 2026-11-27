#pragma once

#include "../UEGameProfile.hpp"
using namespace UEMemory;

class PUBGProfile : public IGameProfile
{
public:
    PUBGProfile() = default;

    static constexpr uintptr_t kGNamesRva           = 0x176E6650;
    static constexpr uintptr_t kGUObjectArrayAuto   = 0x17D46E08;
    static constexpr uintptr_t kGUObjectArrayLegacy = 0x17E8B538;
    static constexpr uintptr_t kPhysxRva            = 0x181A36B0;

    bool ArchSupprted() const override
    {
        return GetUnrealELF().header().e_machine == EM_AARCH64;
    }

    std::string GetAppName() const override
    {
        return "PUBGM private-server current";
    }

    std::vector<std::string> GetAppIDs() const override
    {
        return {"com.tencent.tmgp.pubgmhd"};
    }

    bool isUsingCasePreservingName() const override { return false; }
    bool IsUsingFNamePool() const override { return false; }
    bool isUsingOutlineNumberName() const override { return false; }

    uintptr_t GetNamesPtr() const override
    {
        const uintptr_t base = GetUEVars()->GetBaseAddress();
        const uintptr_t ptr = base + kGNamesRva;
        printf("[PUBGM-QA3] GNames: base+0x%lX = 0x%lX\n",
               (unsigned long)kGNamesRva, (unsigned long)ptr);
        return ptr;
    }

    uintptr_t GetGUObjectArrayPtr() const override
    {
        UE_Offsets *offsets = GetOffsets();
        probeObjectArrayLayout();

        offsets->FUObjectArray.ObjObjects = selectedObjObjectsOff;
        offsets->TUObjectArray.Objects = 0x0;
        offsets->TUObjectArray.NumElements = selectedNumElementsOff;
        offsets->TUObjectArray.NumElementsPerChunk = selectedChunked ? 0x10000 : 0;
        offsets->FUObjectItem.Object = 0x0;
        offsets->FUObjectItem.Size = selectedItemStride;
        offsets->UObject.InternalIndex = selectedInternalIndexOff;
        offsets->UObject.NamePrivate = selectedNamePrivateOff;

        printf("[PUBGM-QA3] selected=%s root=0x%lX ObjOff=0x%lX NumOff=0x%lX "
               "chunked=%d stride=0x%lX InternalIndex=0x%lX NamePrivate=0x%lX score=%d\n",
               selectedLabel,
               (unsigned long)selectedRoot,
               (unsigned long)selectedObjObjectsOff,
               (unsigned long)selectedNumElementsOff,
               selectedChunked ? 1 : 0,
               (unsigned long)selectedItemStride,
               (unsigned long)selectedInternalIndexOff,
               (unsigned long)selectedNamePrivateOff,
               selectedScore);
        return selectedRoot;
    }

    uintptr_t GetFrameCount() const override { return 0; }
    uintptr_t GetMatrix() const override { return 0; }

    uintptr_t GetPhysx() const override
    {
        return GetUEVars()->GetBaseAddress() + kPhysxRva;
    }

    UE_Offsets *GetOffsets() const override
    {
        static UE_Offsets offsets =
            UE_DefaultOffsets::UE4_18_19(isUsingCasePreservingName());

        static bool once = false;
        if (!once)
        {
            once = true;
            offsets.FNameEntry.Index = sizeof(void *);
            offsets.FNameEntry.Name = 0x0C;
            offsets.FUObjectArray.ObjObjects = 0x10;
            offsets.TUObjectArray.Objects = 0x0;
            offsets.TUObjectArray.NumElements = 0x14;
            offsets.TUObjectArray.NumElementsPerChunk = 0x10000;
            offsets.FUObjectItem.Object = 0x0;
            offsets.FUObjectItem.Size = 0x18;
            offsets.UObject.InternalIndex = 0x0C;
            offsets.UObject.NamePrivate = 0x48;
        }
        return &offsets;
    }

private:
    mutable uintptr_t selectedRoot = 0;
    mutable uintptr_t selectedObjObjectsOff = 0x10;
    mutable uintptr_t selectedNumElementsOff = 0x14;
    mutable uintptr_t selectedItemStride = 0x18;
    mutable uintptr_t selectedInternalIndexOff = 0x0C;
    mutable uintptr_t selectedNamePrivateOff = 0x48;
    mutable bool selectedChunked = true;
    mutable int selectedScore = -1;
    mutable const char *selectedLabel = "none";

    static uintptr_t normalizePtr(uintptr_t p)
    {
        if ((p >> 56) == 0xB4)
            p &= 0x00FFFFFFFFFFFFFFULL;
        return p;
    }

    static bool plausibleCount(int32_t v)
    {
        return v >= 10 && v <= 2000000;
    }

    static bool plausibleNameId(int32_t v)
    {
        return v >= 0 && v <= 0x1000000;
    }

    void considerCandidate(const char *label,
                           uintptr_t root,
                           uintptr_t objOff,
                           uintptr_t numOff,
                           bool chunked,
                           uintptr_t stride) const
    {
        if (!root || !kPtrValidator.isPtrReadable(root))
            return;

        const uintptr_t objStruct = root + objOff;
        uintptr_t objectsRaw = 0;
        int32_t count = 0;
        if (!vm_rpm_ptr((void *)objStruct, &objectsRaw, sizeof(objectsRaw)))
            return;
        if (!vm_rpm_ptr((void *)(objStruct + numOff), &count, sizeof(count)))
            return;

        uintptr_t objects = normalizePtr(objectsRaw);
        if (!plausibleCount(count) || !objects || !kPtrValidator.isPtrReadable(objects))
            return;

        int score = 8;
        uintptr_t container = objects;
        if (chunked)
        {
            uintptr_t chunkRaw = 0;
            if (!vm_rpm_ptr((void *)objects, &chunkRaw, sizeof(chunkRaw)))
                return;
            container = normalizePtr(chunkRaw);
            if (!container || !kPtrValidator.isPtrReadable(container))
                return;
            score += 3;
        }

        uintptr_t object1Raw = 0;
        if (!vm_rpm_ptr((void *)(container + stride), &object1Raw, sizeof(object1Raw)))
            return;
        uintptr_t object1 = normalizePtr(object1Raw);
        if (!object1 || !kPtrValidator.isPtrReadable(object1))
            return;
        score += 5;

        uintptr_t foundIndexOff = 0x0C;
        bool foundIndex = false;
        static constexpr uintptr_t indexOffsets[] = {
            0x08, 0x0C, 0x10, 0x18, 0x20, 0x28, 0x38, 0x48, 0x50, 0x58
        };
        for (uintptr_t off : indexOffsets)
        {
            int32_t value = -1;
            if (vm_rpm_ptr((void *)(object1 + off), &value, sizeof(value)) && value == 1)
            {
                foundIndexOff = off;
                foundIndex = true;
                score += 8;
                break;
            }
        }

        uintptr_t foundNameOff = 0x48;
        std::string foundName;
        static constexpr uintptr_t nameOffsets[] = {
            0x18, 0x1C, 0x20, 0x28, 0x38, 0x40, 0x48, 0x50, 0x58, 0x60
        };
        for (uintptr_t off : nameOffsets)
        {
            int32_t id = -1;
            if (!vm_rpm_ptr((void *)(object1 + off), &id, sizeof(id)) || !plausibleNameId(id))
                continue;
            const std::string name = GetNameByID(id);
            if (!name.empty() && name != "None")
            {
                foundNameOff = off;
                foundName = name;
                score += 5;
                break;
            }
        }

        int32_t maxBefore = 0;
        if (numOff >= 4 &&
            vm_rpm_ptr((void *)(objStruct + numOff - 4), &maxBefore, sizeof(maxBefore)) &&
            maxBefore >= count && maxBefore <= 4000000)
            score += 2;

        printf("[PUBGM-QA3] candidate %-18s root=0x%lX ObjOff=0x%lX NumOff=0x%lX "
               "count=%d objects=0x%lX obj1=0x%lX mode=%s stride=0x%lX "
               "idxOff=0x%lX idxOK=%d nameOff=0x%lX name=%s score=%d\n",
               label,
               (unsigned long)root,
               (unsigned long)objOff,
               (unsigned long)numOff,
               count,
               (unsigned long)objects,
               (unsigned long)object1,
               chunked ? "chunked" : "flat",
               (unsigned long)stride,
               (unsigned long)foundIndexOff,
               foundIndex ? 1 : 0,
               (unsigned long)foundNameOff,
               foundName.empty() ? "-" : foundName.c_str(),
               score);

        if (score > selectedScore)
        {
            selectedScore = score;
            selectedRoot = root;
            selectedObjObjectsOff = objOff;
            selectedNumElementsOff = numOff;
            selectedChunked = chunked;
            selectedItemStride = stride;
            selectedInternalIndexOff = foundIndexOff;
            selectedNamePrivateOff = foundNameOff;
            selectedLabel = label;
        }
    }

    void scanRoot(const char *label, uintptr_t root) const
    {
        if (!root || !kPtrValidator.isPtrReadable(root))
        {
            printf("[PUBGM-QA3] root %-18s invalid 0x%lX\n", label, (unsigned long)root);
            return;
        }

        uintptr_t q0 = 0, q8 = 0, q10 = 0, q18 = 0;
        vm_rpm_ptr((void *)(root + 0x00), &q0, sizeof(q0));
        vm_rpm_ptr((void *)(root + 0x08), &q8, sizeof(q8));
        vm_rpm_ptr((void *)(root + 0x10), &q10, sizeof(q10));
        vm_rpm_ptr((void *)(root + 0x18), &q18, sizeof(q18));
        printf("[PUBGM-QA3] root %-18s =0x%lX q0=%016lX q8=%016lX q10=%016lX q18=%016lX\n",
               label, (unsigned long)root,
               (unsigned long)q0, (unsigned long)q8,
               (unsigned long)q10, (unsigned long)q18);

        static constexpr uintptr_t numOffsets[] = {
            0x0C, 0x10, 0x14, 0x18, 0x20, 0x28, 0x30, 0x34,
            0x38, 0x3C, 0x40, 0x48, 0x50, 0x58
        };
        static constexpr uintptr_t strides[] = {0x10, 0x18, 0x20};

        for (uintptr_t objOff = 0; objOff <= 0x200; objOff += 8)
        {
            for (uintptr_t numOff : numOffsets)
            {
                for (uintptr_t stride : strides)
                {
                    considerCandidate(label, root, objOff, numOff, false, stride);
                    considerCandidate(label, root, objOff, numOff, true, stride);
                }
            }
        }
    }

    void probeObjectArrayLayout() const
    {
        selectedScore = -1;

        const uintptr_t base = GetUEVars()->GetBaseAddress();
        const uintptr_t autoRoot = base + kGUObjectArrayAuto;
        const uintptr_t legacyRoot = base + kGUObjectArrayLegacy;

        uintptr_t autoDeref = 0, legacyDeref = 0;
        vm_rpm_ptr((void *)autoRoot, &autoDeref, sizeof(autoDeref));
        vm_rpm_ptr((void *)legacyRoot, &legacyDeref, sizeof(legacyDeref));
        autoDeref = normalizePtr(autoDeref);
        legacyDeref = normalizePtr(legacyDeref);

        uintptr_t signatureRoot = 0, signatureDeref = 0;
        const std::string pattern =
            "? ? ? B9 ? ? ? B9 1F 01 09 6B ? ? ? 54 ? ? ? ? ? ? ? 91 "
            "E1 03 14 AA ? ? ? 97 ? ? ? 91 ? ? ? 91";
        const PATTERN_MAP_TYPE mapType =
            isEmulator() ? PATTERN_MAP_TYPE::ANY_R : PATTERN_MAP_TYPE::ANY_X;
        const uintptr_t match = findIdaPattern(mapType, pattern, 0x10);
        if (match)
        {
            const uintptr_t decoded = Arm64::Decode_ADRP_ADD(match);
            if (decoded)
                signatureRoot = decoded + 0xB8;
        }
        if (signatureRoot && kPtrValidator.isPtrReadable(signatureRoot))
        {
            vm_rpm_ptr((void *)signatureRoot, &signatureDeref, sizeof(signatureDeref));
            signatureDeref = normalizePtr(signatureDeref);
        }

        printf("[PUBGM-QA3] roots auto=0x%lX ->0x%lX legacy=0x%lX ->0x%lX "
               "sigMatch=0x%lX sigRoot=0x%lX ->0x%lX\n",
               (unsigned long)autoRoot, (unsigned long)autoDeref,
               (unsigned long)legacyRoot, (unsigned long)legacyDeref,
               (unsigned long)match, (unsigned long)signatureRoot,
               (unsigned long)signatureDeref);

        scanRoot("autofix", autoRoot);
        scanRoot("autofix-deref", autoDeref);
        scanRoot("legacy", legacyRoot);
        scanRoot("legacy-deref", legacyDeref);
        scanRoot("signature", signatureRoot);
        scanRoot("signature-deref", signatureDeref);

        if (selectedScore < 0)
        {
            selectedRoot = autoRoot;
            selectedObjObjectsOff = 0x10;
            selectedNumElementsOff = 0x14;
            selectedChunked = true;
            selectedItemStride = 0x18;
            selectedInternalIndexOff = 0x0C;
            selectedNamePrivateOff = 0x48;
            selectedLabel = "no-valid-layout";
            selectedScore = 0;
        }
    }
};
