#pragma once

#include "../UEGameProfile.hpp"
using namespace UEMemory;

class PUBGProfile : public IGameProfile
{
public:
    PUBGProfile() = default;

    static constexpr uintptr_t kGNamesRva          = 0x176E6650;
    static constexpr uintptr_t kGUObjectArrayAuto  = 0x17D46E08;
    static constexpr uintptr_t kGUObjectArrayLegacy= 0x17E8B538;
    static constexpr uintptr_t kPhysxRva           = 0x181A36B0;

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

    uintptr_t GetGUObjectArrayPtr() const override
    {
        const auto base = GetUEVars()->GetBaseAddress();
        const auto ptr = base + selectedGUObjectRva;
        printf("[PUBGM-QA2] Selected %s\n", selectedLayoutName);
        printf("[PUBGM-QA2] GUObjectArray: base+0x%lX = 0x%lX\n",
               (unsigned long)selectedGUObjectRva, (unsigned long)ptr);
        printf("[PUBGM-QA2] ObjObjectsOff=0x%lX NumElementsOff=0x%lX Chunk=%d\n",
               (unsigned long)selectedObjObjectsOff,
               (unsigned long)selectedNumElementsOff,
               selectedNumElementsPerChunk);
        return ptr;
    }

    uintptr_t GetNamesPtr() const override
    {
        const auto base = GetUEVars()->GetBaseAddress();
        const auto ptr = base + kGNamesRva;
        printf("[PUBGM-QA2] GNames: base+0x%lX = 0x%lX\n",
               (unsigned long)kGNamesRva, (unsigned long)ptr);
        printf("[PUBGM-QA2] UObject.NamePrivate=0x48 FNameEntry.Name=0x0C\n");
        return ptr;
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
            offsets.UObject.NamePrivate = 0x48;
            offsets.FUObjectItem.Object = 0x0;
            offsets.FUObjectItem.Size = 0x18;

            probeObjectArrayLayout();

            offsets.FUObjectArray.ObjObjects = selectedObjObjectsOff;
            offsets.TUObjectArray.Objects = 0x0;
            offsets.TUObjectArray.NumElements = selectedNumElementsOff;
            offsets.TUObjectArray.NumElementsPerChunk =
                selectedNumElementsPerChunk;
        }

        return &offsets;
    }

private:
    struct ObjectLayoutCandidate
    {
        const char *name;
        uintptr_t guRva;
        uintptr_t objObjectsOff;
        uintptr_t numElementsOff;
        int32_t numElementsPerChunk;
    };

    mutable uintptr_t selectedGUObjectRva = kGUObjectArrayAuto;
    mutable uintptr_t selectedObjObjectsOff = 0x10;
    mutable uintptr_t selectedNumElementsOff = 0x14;
    mutable int32_t selectedNumElementsPerChunk = 0x10000;
    mutable const char *selectedLayoutName = "autofix-standard-chunked";

    static uintptr_t untagPointer(uintptr_t p)
    {
        if ((p >> 56) == 0xB4)
            p &= 0x00FFFFFFFFFFFFFFULL;
        return p;
    }

    static bool plausibleCount(int32_t v)
    {
        return v >= 10 && v <= 2000000;
    }

    int scoreCandidate(const ObjectLayoutCandidate &c) const
    {
        const uintptr_t base = GetUEVars()->GetBaseAddress();
        const uintptr_t obj = base + c.guRva + c.objObjectsOff;

        int32_t count = 0;
        int32_t before = 0;
        uintptr_t objects = 0;

        const bool countRead =
            vm_rpm_ptr((void *)(obj + c.numElementsOff), &count, sizeof(count));
        const bool beforeRead = c.numElementsOff >= 4 &&
            vm_rpm_ptr((void *)(obj + c.numElementsOff - 4), &before, sizeof(before));
        const bool objectsRead =
            vm_rpm_ptr((void *)obj, &objects, sizeof(objects));

        objects = untagPointer(objects);

        int score = 0;
        if (countRead && plausibleCount(count)) score += 5;
        if (beforeRead && plausibleCount(count) && before >= count && before <= 4000000)
            score += 2;
        if (objectsRead && objects && kPtrValidator.isPtrReadable(objects))
            score += 3;

        uintptr_t object1 = 0;
        uintptr_t firstContainer = 0;
        if (objectsRead && objects && kPtrValidator.isPtrReadable(objects))
        {
            if (c.numElementsPerChunk > 0)
            {
                firstContainer = untagPointer(vm_rpm_ptr<uintptr_t>((void *)objects));
                if (firstContainer && kPtrValidator.isPtrReadable(firstContainer))
                {
                    score += 2;
                    object1 = untagPointer(vm_rpm_ptr<uintptr_t>(
                        (void *)(firstContainer + 0x18)));
                }
            }
            else
            {
                firstContainer = objects;
                object1 = untagPointer(vm_rpm_ptr<uintptr_t>(
                    (void *)(objects + 0x18)));
            }
        }

        int32_t index8 = -1, indexC = -1, index10 = -1;
        if (object1 && kPtrValidator.isPtrReadable(object1))
        {
            score += 3;
            vm_rpm_ptr((void *)(object1 + 0x8), &index8, sizeof(index8));
            vm_rpm_ptr((void *)(object1 + 0xC), &indexC, sizeof(indexC));
            vm_rpm_ptr((void *)(object1 + 0x10), &index10, sizeof(index10));
            if (index8 == 1 || indexC == 1 || index10 == 1)
                score += 4;
        }

        printf("[PUBGM-QA2] probe %-26s GU=0x%lX ObjOff=0x%lX NumOff=0x%lX "
               "chunks=%d count=%d prev=%d objects=0x%lX obj1=0x%lX "
               "idx8=%d idxC=%d idx10=%d score=%d\n",
               c.name,
               (unsigned long)c.guRva,
               (unsigned long)c.objObjectsOff,
               (unsigned long)c.numElementsOff,
               c.numElementsPerChunk,
               count, before,
               (unsigned long)objects,
               (unsigned long)object1,
               index8, indexC, index10, score);
        return score;
    }

    void probeObjectArrayLayout() const
    {
        // Two independently observed roots exist for this build:
        //  1) AutoFix: GUObjectArray 0x17D46E08 / ObjObjects +0x10.
        //  2) Maintained kp7742 GFP 1.39 path: 0x17E8B538 / +0xC8.
        // Probe both, plus standard UE4 chunked/flat count positions, and pick
        // the layout that produces a plausible count and readable UObject #1.
        static constexpr ObjectLayoutCandidate candidates[] = {
            {"autofix-chunked-14", kGUObjectArrayAuto,   0x10, 0x14, 0x10000},
            {"autofix-flat-14",    kGUObjectArrayAuto,   0x10, 0x14, 0},
            {"autofix-flat-38",    kGUObjectArrayAuto,   0x10, 0x38, 0},
            {"autofix-flat-0C",    kGUObjectArrayAuto,   0x10, 0x0C, 0},
            {"legacy-gfp-flat-38", kGUObjectArrayLegacy, 0xC8, 0x38, 0}
        };

        int bestScore = -1;
        const ObjectLayoutCandidate *best = &candidates[0];
        for (const auto &candidate : candidates)
        {
            const int score = scoreCandidate(candidate);
            if (score > bestScore)
            {
                bestScore = score;
                best = &candidate;
            }
        }

        selectedGUObjectRva = best->guRva;
        selectedObjObjectsOff = best->objObjectsOff;
        selectedNumElementsOff = best->numElementsOff;
        selectedNumElementsPerChunk = best->numElementsPerChunk;
        selectedLayoutName = best->name;

        printf("[PUBGM-QA2] best layout=%s score=%d\n",
               selectedLayoutName, bestScore);
    }
};
