#pragma once

#include <cstdio>
#include <algorithm>
#include <cstring>
#include <vector>

#include "../UEGameProfile.hpp"
using namespace UEMemory;

class DeltaForceProfile : public IGameProfile
{
public:
    DeltaForceProfile() = default;

    // Explicit opt-in: observed RVAs for current QA runtime only.
    void EnableQARuntimeDump(bool enabled) { qaRuntimeDumpEnabled = enabled; }
private:
    bool qaRuntimeDumpEnabled = false;
    static constexpr uintptr_t kQANamePoolRVA = 0x20291EC0;
    static constexpr uintptr_t kQAGUObjectArrayRVA = 0x202B8CF8;

    uintptr_t GetCheckedQANames() const
    {
        const auto elf = GetUnrealELF();
        if (!elf.isValid()) return 0;
        const uintptr_t pool = elf.base() + kQANamePoolRVA;
        const UE_Offsets *off = GetOffsets();
        if (!kPtrValidator.isPtrReadable((void *)pool)) return 0;
        const uintptr_t block0 = vm_rpm_ptr<uintptr_t>(
            (void *)(pool + off->FNamePool.BlocksOff));
        if (!block0 || !kPtrValidator.isPtrReadable((void *)block0)) return 0;
        uint16_t header = 0;
        if (!vm_rpm_ptr((void *)(block0 + off->FNamePoolEntry.Header),
                        &header, sizeof(header))) return 0;
        if (off->FNamePoolEntry.GetLength(header) != 4 ||
            GetNameEntryString((uint8_t *)block0) != "None")
        {
            std::printf("QA_DFM DUMP_PRECHECK FAIL NamePool decode\n");
            return 0;
        }
        std::printf("QA_DFM DUMP_PRECHECK PASS NamePool rva=0x%llx first=None\n",
                    (unsigned long long)kQANamePoolRVA);
        return pool;
    }

    uintptr_t GetCheckedQAObjects() const
    {
        const auto elf = GetUnrealELF();
        if (!elf.isValid()) return 0;
        const UE_Offsets *o = GetOffsets();
        const uintptr_t candidate = elf.base() + kQAGUObjectArrayRVA;
        const uintptr_t arrayBase = candidate + o->FUObjectArray.ObjObjects;
        if (!kPtrValidator.isPtrReadable((void *)arrayBase)) return 0;
        const int32_t num = vm_rpm_ptr<int32_t>(
            (void *)(arrayBase + o->TUObjectArray.NumElements));
        const uintptr_t chunks = vm_rpm_ptr<uintptr_t>(
            (void *)(arrayBase + o->TUObjectArray.Objects));
        if (num < 1024 || num > 3000000 || !chunks ||
            !kPtrValidator.isPtrReadable((void *)chunks))
        {
            std::printf("QA_DFM DUMP_PRECHECK FAIL objects count=%d chunks=0x%llx\n",
                        num, (unsigned long long)chunks);
            return 0;
        }
        const uintptr_t chunk0 = vm_rpm_ptr<uintptr_t>((void *)chunks);
        if (!chunk0 || !kPtrValidator.isPtrReadable((void *)chunk0)) return 0;
        unsigned valid = 0, readableNames = 0;
        for (int k = 0; k < 24; ++k)
        {
            const uintptr_t obj = vm_rpm_ptr<uintptr_t>(
                (void *)(chunk0 + uintptr_t(k) * o->FUObjectItem.Size +
                         o->FUObjectItem.Object));
            if (!obj || !kPtrValidator.isPtrReadable((void *)obj)) continue;
            const uintptr_t klass = vm_rpm_ptr<uintptr_t>(
                (void *)(obj + o->UObject.ClassPrivate));
            const int32_t index = vm_rpm_ptr<int32_t>(
                (void *)(obj + o->UObject.InternalIndex));
            if (!klass || !kPtrValidator.isPtrReadable((void *)klass)
                || index != k) continue;
            ++valid;
            const int32_t nameIndex = vm_rpm_ptr<int32_t>(
                (void *)(obj + o->UObject.NamePrivate));
            if (nameIndex < 0 || nameIndex > 100000000) continue;
            const std::string name = GetNameByID(nameIndex);
            if (name.empty() || name.size() > 120) continue;
            bool printable = true;
            for (unsigned char ch : name)
                if (ch < 32 || ch > 126) { printable = false; break; }
            if (printable) ++readableNames;
            if (printable && k < 5)
                std::printf("QA_DFM DUMP_OBJECT index=%d id=%d name='%s'\n",
                            k, nameIndex, name.c_str());
        }
        std::printf("QA_DFM DUMP_PRECHECK objects_rva=0x%llx count=%d class_indices=%u/24 names=%u/24\n",
                    (unsigned long long)kQAGUObjectArrayRVA,
                    num, valid, readableNames);
        if (valid < 20 || readableNames < 5)
        {
            std::printf("QA_DFM DUMP_PRECHECK FAIL object names or structure\n");
            return 0;
        }
        std::printf("QA_DFM DUMP_PRECHECK PASS GUObjectArray\n");
        return candidate;
    }
public:



    // Runtime-only QA: uses this profile's signature engine and UE4 object
    // layout; no fixed version RVAs, memory writes or SDK generation.
    bool PrintQAPatternDiagnostics() const
    {
        const auto elf = GetUnrealELF();
        if (!elf.isValid())
        {
            std::printf("QA_DFM ERROR no UE ELF mapping\n");
            return false;
        }
        const uintptr_t base = elf.base();
        const auto mode = isEmulator() ? PATTERN_MAP_TYPE::ANY_R : PATTERN_MAP_TYPE::ANY_X;
        std::printf("QA_DFM base=0x%llx mode=%s\n",
                    (unsigned long long)base, isEmulator() ? "readable" : "executable");

        struct Sig { const char *name; const char *bytes; int step; };
        const Sig objectSigs[] = {
            {"GUObjects_1", "91 E1 03 ? AA E0 03 08 AA E2 03 1F 2A", -7},
            {"GUObjects_2", "B4 21 0C 40 B9 ? ? ? ? ? ? ? 91", 5},
            {"GUObjects_3", "9F E5 00 ? 00 E3 FF ? 40 E3 ? ? A0 E1", -2},
            {"GUObjects_4", "96 df 02 17 ? ? ? ? 54 ? ? ? ? ? ? ? 91 e1 03 13 aa", 9},
            {"GUObjects_5", "f4 03 01 2a ? 00 00 34 ? ? ? ? ? ? ? ? ? ? 00 54 ? 00 00 14 ? ? ? ? ? ? ? 91", 0x18},
            {"GUObjects_6", "69 3e 40 b9 1f 01 09 6b ? ? ? 54 e1 03 13 aa ? ? ? ? f4 4f ? a9 ? ? ? ? ? ? ? 91", 0x18},
        };
        unsigned matchedSigs = 0;
        for (const auto &sig : objectSigs)
        {
            const uintptr_t ip = findIdaPattern(mode, sig.bytes, sig.step);
            const uintptr_t ptr = Arm64::Decode_ADRP_ADD(ip);
            const bool valid = ptr && kPtrValidator.isPtrReadable((const void *)ptr);
            std::printf("QA_DFM %-12s hit_rva=0x%llx candidate_rva=0x%llx readable=%d\n",
                        sig.name,
                        (unsigned long long)(ip && ip >= base ? ip - base : 0),
                        (unsigned long long)(ptr && ptr >= base ? ptr - base : 0),
                        valid ? 1 : 0);
            matchedSigs += valid ? 1 : 0;
        }

        const uintptr_t nameInsn = findIdaPattern(mode, "91 ? 10 81 52 ? ? 21 8b", -7);
        const uintptr_t pool = Arm64::Decode_ADRP_ADD(nameInsn);
        const bool poolReadable = pool && kPtrValidator.isPtrReadable((const void *)pool);
        std::printf("QA_DFM NamePool hit_rva=0x%llx candidate_rva=0x%llx readable=%d\n",
                    (unsigned long long)(nameInsn && nameInsn >= base ? nameInsn - base : 0),
                    (unsigned long long)(pool && pool >= base ? pool - base : 0),
                    poolReadable ? 1 : 0);
        bool decodedName = false;
        if (poolReadable)
        {
            // A readable global is insufficient. Follow real DeltaForce FNamePool
            // layout and run its own GetNameEntryString() decoder.
            const UE_Offsets *off = GetOffsets();
            const uintptr_t block0 =
                vm_rpm_ptr<uintptr_t>((void *)(pool + off->FNamePool.BlocksOff));
            uint32_t currentBlock = 0, cursor = 0;
            vm_rpm_ptr((void *)(pool + sizeof(void *)), &currentBlock, sizeof(currentBlock));
            vm_rpm_ptr((void *)(pool + sizeof(void *) + sizeof(uint32_t)), &cursor, sizeof(cursor));
            if (block0 && kPtrValidator.isPtrReadable((const void *)block0))
            {
                uint16_t header = 0;
                vm_rpm_ptr((const void *)(block0 + off->FNamePoolEntry.Header),
                           &header, sizeof(header));
                const size_t length = off->FNamePoolEntry.GetLength(header);
                std::string firstName;
                if (length > 0 && length <= 96)
                    firstName = GetNameEntryString((uint8_t *)block0);
                for (char &c : firstName)
                    if ((unsigned char)c < 32 || (unsigned char)c > 126)
                        c = '?';
                decodedName = !firstName.empty() && firstName != "None" ?
                              firstName.size() >= 2 : firstName == "None";
                std::printf("QA_DFM POOL_CHECK block0=0x%llx current_block=%u cursor=0x%x header=0x%x length=%zu name0='%s' decode_plausible=%d\n",
                            (unsigned long long)block0, currentBlock, cursor,
                            header, length, firstName.c_str(), decodedName ? 1 : 0);
            }
            else
            {
                std::printf("QA_DFM POOL_CHECK invalid first block pointer=0x%llx\n",
                            (unsigned long long)block0);
            }
        }

        // The six upstream signatures may be version-specific. Use the native
        // DeltaForce TUObjectArray offsets to cross-check .bss/data ONLY.
        // No blind process-wide sweep. Candidates are evidence, not an
        // automatic patch to the production object-array address.
        const UE_Offsets *o = GetOffsets();
        const size_t nOff = o->FUObjectArray.ObjObjects + o->TUObjectArray.NumElements;
        const size_t pOff = o->FUObjectArray.ObjObjects + o->TUObjectArray.Objects;
        const size_t itemSize = o->FUObjectItem.Size;
        const size_t indexOff = o->UObject.InternalIndex;
        const size_t classOff = o->UObject.ClassPrivate;
        std::printf("QA_DFM LAYOUT num_off=0x%zx objects_off=0x%zx item_stride=0x%zx class_off=0x%zx index_off=0x%zx\n",
                    nOff, pOff, itemSize, classOff, indexOff);
        std::vector<KittyMemoryEx::ProcMap> regions = elf.bssSegments();
        std::vector<KittyMemoryEx::ProcMap> writable;
        for (const auto &seg : elf.segments())
            if (seg.readable && seg.writeable && seg.is_private)
                writable.push_back(seg);
        // Scan high-address writable data before large low-address RELRO.
        std::sort(writable.begin(), writable.end(),
                  [](const KittyMemoryEx::ProcMap &a,
                     const KittyMemoryEx::ProcMap &b) { return a.startAddress > b.startAddress; });
        for (const auto &seg : writable)
        {
            bool overlaps = false;
            for (const auto &bss : regions)
                if (seg.startAddress < bss.endAddress && bss.startAddress < seg.endAddress)
                    overlaps = true;
            if (!overlaps) regions.push_back(seg);
        }
        size_t bytesRead = 0;
        unsigned considered = 0, validCandidates = 0;
        const size_t budget = 32 * 1024 * 1024;
        std::vector<uint8_t> buf(64 * 1024 + 64);
        for (const auto &region : regions)
        {
            if (!region.readable || region.endAddress <= region.startAddress || bytesRead >= budget)
                continue;
            std::printf("QA_DFM REGION start_rva=0x%llx bytes=%llu\n",
                        (unsigned long long)(region.startAddress >= base ? region.startAddress - base : 0),
                        (unsigned long long)(region.endAddress - region.startAddress));
            for (uintptr_t addr = region.startAddress;
                 addr < region.endAddress && bytesRead < budget;)
            {
                const size_t available = (size_t)(region.endAddress - addr);
                const size_t count = std::min<size_t>(buf.size(), std::min<size_t>(available, budget - bytesRead));
                if (count < pOff + sizeof(uintptr_t)) break;
                if (kMgr.readMem(addr, buf.data(), count) == count)
                {
                    // A GUObjectArray is pointer-aligned. Check count, its chunk
                    // table, real object class and InternalIndex relationships.
                    for (size_t at = 0; at + pOff + 8 <= count; at += 8)
                    {
                        int32_t num = 0;
                        uintptr_t chunks = 0;
                        std::memcpy(&num, buf.data() + at + nOff, sizeof(num));
                        if (num < 1024 || num > 3000000) continue;
                        std::memcpy(&chunks, buf.data() + at + pOff, sizeof(chunks));
                        if (!chunks || chunks % sizeof(void *) != 0 ||
                            !kPtrValidator.isPtrReadable((const void *)chunks))
                            continue;
                        ++considered;
                        const uintptr_t chunk0 = vm_rpm_ptr<uintptr_t>((void *)chunks);
                        if (!chunk0 || !kPtrValidator.isPtrReadable((const void *)chunk0))
                            continue;
                        unsigned score = 0;
                        for (int32_t k = 0; k < 24; ++k)
                        {
                            const uintptr_t obj = vm_rpm_ptr<uintptr_t>(
                                (void *)(chunk0 + (uintptr_t)k * itemSize + o->FUObjectItem.Object));
                            if (!obj || !kPtrValidator.isPtrReadable((const void *)obj))
                                continue;
                            const uintptr_t klass = vm_rpm_ptr<uintptr_t>((void *)(obj + classOff));
                            const int32_t objectIndex = vm_rpm_ptr<int32_t>((void *)(obj + indexOff));
                            if (klass && kPtrValidator.isPtrReadable((const void *)klass)
                                && objectIndex == k)
                                ++score;
                        }
                        if (score >= 3)
                        {
                            ++validCandidates;
                            const uintptr_t guessed = addr + at;
                            std::printf("QA_DFM OBJECT_LAYOUT_CANDIDATE rva=0x%llx count=%d validated_indices=%u/24 chunks=0x%llx\n",
                                        (unsigned long long)(guessed >= base ? guessed - base : 0),
                                        num, score, (unsigned long long)chunks);
                            if (validCandidates >= 12) break;
                        }
                    }
                }
                bytesRead += count;
                if (validCandidates >= 12) break;
                const size_t step = count > 64 ? count - 64 : count;
                addr += step;
            }
            if (validCandidates >= 12) break;
        }
        std::printf("QA_DFM SUMMARY sig_guobjects=%u namepool_readable=%d name0_plausible=%d structural_objects=%u considered=%u scanned_bytes=%zu\n",
                    matchedSigs, poolReadable ? 1 : 0, decodedName ? 1 : 0,
                    validCandidates, considered, bytesRead);
        std::printf("QA_DFM STATUS no automatic offset changes; candidates require independent validation\n");
        return decodedName && (matchedSigs > 0 || validCandidates > 0);
    }

    bool ArchSupprted() const override
    {
        auto e_machine = GetUnrealELF().header().e_machine;
        return e_machine == EM_AARCH64;
    }

    std::string GetAppName() const override
    {
        return "Delta Force(CN)";
    }

    std::vector<std::string> GetAppIDs() const override
    {
        return {"com.tencent.tmgp.dfm"};
    }

    bool isUsingCasePreservingName() const override
    {
        return false;
    }

    bool IsUsingFNamePool() const override
    {
        return true;
    }

    bool isUsingOutlineNumberName() const override
    {
        return false;
    }

    uintptr_t GetGUObjectArrayPtr() const override
    {
        if (qaRuntimeDumpEnabled) return GetCheckedQAObjects();
        std::vector<std::pair<std::string, int>> idaPatterns = {
            {"91 E1 03 ? AA E0 03 08 AA E2 03 1F 2A", -7},
            {"B4 21 0C 40 B9 ? ? ? ? ? ? ? 91", 5},
            {"9F E5 00 ? 00 E3 FF ? 40 E3 ? ? A0 E1", -2},
            {"96 df 02 17 ? ? ? ? 54 ? ? ? ? ? ? ? 91 e1 03 13 aa", 9},
            {"f4 03 01 2a ? 00 00 34 ? ? ? ? ? ? ? ? ? ? 00 54 ? 00 00 14 ? ? ? ? ? ? ? 91", 0x18},
            {"69 3e 40 b9 1f 01 09 6b ? ? ? 54 e1 03 13 aa ? ? ? ? f4 4f ? a9 ? ? ? ? ? ? ? 91", 0x18},
        };

        PATTERN_MAP_TYPE map_type = isEmulator() ? PATTERN_MAP_TYPE::ANY_R : PATTERN_MAP_TYPE::ANY_X;

        for (const auto &it : idaPatterns)
        {
            std::string ida_pattern = it.first;
            const int step = it.second;

            uintptr_t adrl = Arm64::Decode_ADRP_ADD(findIdaPattern(map_type, ida_pattern, step));
            if (adrl != 0) return adrl;
        }

        return 0;
    }
    uintptr_t GetFrameCount() const override
    {
        PATTERN_MAP_TYPE map_type = isEmulator() ? PATTERN_MAP_TYPE::ANY_R : PATTERN_MAP_TYPE::ANY_X;

        std::string ida_pattern = "C0 03 5F D6 ? ? ? ? 08 ? ? ? 00 01 40 F9 C0 03 5F D6 FD 7B BF A9";
        const int step = 4;
        auto FrameOff = Arm64::Decode_ADRP_LDR(findIdaPattern(map_type, ida_pattern, step));
        if (FrameOff !=0)
            return vm_rpm_ptr<uintptr_t>((void*)FrameOff);
        return  0;
    }
    uintptr_t GetMatrix() const override
    {
        std::vector<std::pair<std::string, int>> idaPatterns = {
            {"08 3D 40 F9 00 01 3F D6 E8 03 13 AA ? ? ? F9", 0x18},
            {"FD ? ? A9 28 ? ? F9 F3 ? ? F8 C0 03 5F D6", -0x1c},
            {"00 01 3F D6 E8 03 13 AA 60 ? 00 F9 ? ? ? A9", 14},
        };

        PATTERN_MAP_TYPE map_type = isEmulator() ? PATTERN_MAP_TYPE::ANY_R : PATTERN_MAP_TYPE::ANY_X;

        for (const auto &it : idaPatterns)
        {
            std::string ida_pattern = it.first;
            const int step = it.second;

            uintptr_t adrl = Arm64::Decode_ADRP_LDR(findIdaPattern(map_type, ida_pattern, step));
            if (adrl != 0) return adrl;
        }
        return 0;
    }
    uintptr_t GetPhysx() const override
    {
        std::vector<std::pair<std::string, int>> idaPatterns = {
            {"E1 ? ? ? 40 00 40 BD F4 03 02 AA",0x28},
            {"48 ? ? ? F3 03 04 AA F5 03 03 2A", -0xc},
        };

        PATTERN_MAP_TYPE map_type = isEmulator() ? PATTERN_MAP_TYPE::ANY_R : PATTERN_MAP_TYPE::ANY_X;

        for (const auto &it : idaPatterns)
        {
            std::string ida_pattern = it.first;
            const int step = it.second;

            uintptr_t adrl = Arm64::Decode_ADRP_LDR(findIdaPattern(map_type, ida_pattern, step),8);
            //printf("%lx\n", vm_rpm_ptr<uintptr_t>((void*)adrl));
            if (adrl != 0) return vm_rpm_ptr<uintptr_t>((void*)adrl);
        }
        return 0;
    }
    uintptr_t GetNamesPtr() const override
    {
        if (qaRuntimeDumpEnabled) return GetCheckedQANames();
        PATTERN_MAP_TYPE map_type = isEmulator() ? PATTERN_MAP_TYPE::ANY_R : PATTERN_MAP_TYPE::ANY_X;

        std::string ida_pattern = "91 ? 10 81 52 ? ? 21 8b";
        const int step = -7;

        return Arm64::Decode_ADRP_ADD(findIdaPattern(map_type, ida_pattern, step));
    }

    UE_Offsets *GetOffsets() const override
    {
        static UE_Offsets offsets = UE_DefaultOffsets::UE4_25_27(isUsingCasePreservingName());

        static bool once = false;
        if (!once)
        {
            once = true;

            offsets.UEnum.Names = 0x40;

            offsets.FNamePool.BlocksBit = 18;
            offsets.FNamePool.BlocksOff -= sizeof(void *);

            offsets.TUObjectArray.NumElements = sizeof(int32_t);
            offsets.TUObjectArray.Objects = offsets.TUObjectArray.NumElements + (sizeof(int32_t) * 3);
            offsets.TUObjectArray.NumElementsPerChunk = 0x10000;

            offsets.UObject.ClassPrivate = sizeof(void *);
            offsets.UObject.OuterPrivate = offsets.UObject.ClassPrivate + sizeof(void *);
            offsets.UObject.ObjectFlags = offsets.UObject.OuterPrivate + sizeof(void *);
            offsets.UObject.NamePrivate = offsets.UObject.ObjectFlags + sizeof(int32_t);
            offsets.UObject.InternalIndex = offsets.UObject.NamePrivate + offsets.FName.Size;

            offsets.UField.Next = 0x28;
            offsets.UStruct.PropertiesSize = offsets.UField.Next + (sizeof(void *) * 2) + sizeof(int32_t);
            offsets.UStruct.SuperStruct = offsets.UStruct.PropertiesSize + sizeof(int32_t);
            offsets.UStruct.Children = offsets.UStruct.SuperStruct + (sizeof(void *) * 2);
            offsets.UStruct.ChildProperties = offsets.UStruct.Children + (sizeof(void *) * 3);

            offsets.UFunction.NumParams = 0xb0;
            offsets.UFunction.ParamSize = 0xb2;
            offsets.UFunction.EFunctionFlags = 0xb8;
            offsets.UFunction.Func = 0xd8;

            offsets.FField.FlagsPrivate = sizeof(void *);
            offsets.FField.Next = offsets.FField.FlagsPrivate + (sizeof(void *) * 2);
            offsets.FField.ClassPrivate = offsets.FField.Next + sizeof(void *);
            offsets.FField.NamePrivate = offsets.FField.ClassPrivate + sizeof(void *);

            offsets.FProperty.ArrayDim = offsets.FField.NamePrivate + GetPtrAlignedOf(offsets.FName.Size) + sizeof(void *);
            offsets.FProperty.ElementSize = offsets.FProperty.ArrayDim + sizeof(int32_t);
            offsets.FProperty.PropertyFlags = offsets.FProperty.ElementSize + sizeof(int32_t);
            offsets.FProperty.Offset_Internal = offsets.FProperty.PropertyFlags + sizeof(int64_t) + sizeof(int32_t);
            offsets.FProperty.Size = 0x80;

            /*
                        offsets.UObject.ClassPrivate = 0x18;
                        offsets.UObject.OuterPrivate = 0x20;
                        offsets.UObject.ObjectFlags = 0x28;
                        offsets.UObject.NamePrivate = 0x2c;
                        offsets.UObject.InternalIndex = 0x34;

                        offsets.UField.Next = 0x30;
                        offsets.UStruct.PropertiesSize = 0x4c;
                        offsets.UStruct.SuperStruct = 0x48+0x8;
                        offsets.UStruct.Children = 0x58+0x8;
                        offsets.UStruct.ChildProperties = 0x70+0x8;

                        offsets.UFunction.NumParams = 0xc4;
                        offsets.UFunction.ParamSize = 0xc6;
                        offsets.UFunction.EFunctionFlags = 0xcc;
                        offsets.UFunction.Func = 0xf0;



                        offsets.FField.FlagsPrivate = sizeof(void *);
                        offsets.FField.Next = offsets.FField.FlagsPrivate + (sizeof(void *) * 2);
                        offsets.FField.ClassPrivate = offsets.FField.Next + sizeof(void *);
                        offsets.FField.NamePrivate = offsets.FField.ClassPrivate + sizeof(void *);

                        offsets.FProperty.ArrayDim = offsets.FField.NamePrivate + GetPtrAlignedOf(offsets.FName.Size) + sizeof(void *);
                        offsets.FProperty.ElementSize = offsets.FProperty.ArrayDim + sizeof(int32_t);
                        offsets.FProperty.PropertyFlags = offsets.FProperty.ElementSize + sizeof(int32_t);
                        offsets.FProperty.Offset_Internal = offsets.FProperty.PropertyFlags + sizeof(int64_t) + sizeof(int32_t);
                        offsets.FProperty.Size = offsets.FProperty.Offset_Internal + (sizeof(int32_t) * 3) + (sizeof(void *) * 4);

                        offsets.UProperty.ArrayDim = offsets.UField.Next + sizeof(void *);  // sizeof(UField)
                        offsets.UProperty.ElementSize = offsets.UProperty.ArrayDim + sizeof(int32_t);
                        offsets.UProperty.PropertyFlags = GetPtrAlignedOf(offsets.UProperty.ElementSize + sizeof(int32_t));
                        offsets.UProperty.Offset_Internal = offsets.UProperty.PropertyFlags + sizeof(int64_t) + sizeof(int32_t);
                        offsets.UProperty.Size = GetPtrAlignedOf(offsets.UProperty.Offset_Internal + sizeof(int32_t) + offsets.FName.Size) + (sizeof(void *) * 4);  // sizeof(UProperty)*/
        }

        return &offsets;
    }

    std::string GetNameEntryString(uint8_t *entry) const override
    {
        std::string name = IGameProfile::GetNameEntryString(entry);

        auto dec_ansi = [](char *str, uint32_t len)
        {
            if (!str || !*str || len == 0) return;

            uint32_t key = 0;
            switch (len % 9)
            {
            case 0u:
                key = ((len & 0x1F) + len);
                break;
            case 1u:
                key = ((len ^ 0xDF) + len);
                break;
            case 2u:
                key = ((len | 0xCF) + len);
                break;
            case 3u:
                key = (33 * len);
                break;
            case 4u:
                key = (len + (len >> 2));
                break;
            case 5u:
                key = (3 * len + 5);
                break;
            case 6u:
                key = (((4 * len) | 5) + len);
                break;
            case 7u:
                key = (((len >> 4) | 7) + len);
                break;
            case 8u:
                key = ((len ^ 0xC) + len);
                break;
            default:
                key = ((len ^ 0x40) + len);
                break;
            }

            for (uint32_t i = 0; i < len; i++)
            {
                str[i] = (key & 0x80) ^ ~str[i];
            }
        };

        dec_ansi(name.data(), uint32_t(name.length()));

        return name;
    }
};