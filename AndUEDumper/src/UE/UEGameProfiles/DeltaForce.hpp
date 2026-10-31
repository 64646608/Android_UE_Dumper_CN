#pragma once

#include "../UEGameProfile.hpp"
using namespace UEMemory;

class DeltaForceProfile : public IGameProfile
{
public:
    DeltaForceProfile() = default;

    // Read-only dynamic signature diagnostic for this concrete game's runtime
    // image. It deliberately does not use previously reported fixed RVAs and
    // does not attempt an SDK dump or write memory.
    bool PrintQAPatternDiagnostics() const
    {
        auto elf = GetUnrealELF();
        if (!elf.isValid())
        {
            std::printf("QA_DFM ERROR: libUE4.so mapping not found\n");
            return false;
        }

        const uintptr_t base = elf.base();
        const PATTERN_MAP_TYPE map_type =
            isEmulator() ? PATTERN_MAP_TYPE::ANY_R : PATTERN_MAP_TYPE::ANY_X;
        std::printf("QA_DFM base=0x%llx scan_mode=%s profile=DeltaForce\n",
                    (unsigned long long)base,
                    isEmulator() ? "readable" : "executable");
        std::printf("QA_DFM NOTE: signature candidates are NOT validated UE4 globals\n");

        struct Signature
        {
            const char *label;
            const char *pattern;
            int step;
        };
        const Signature objectSignatures[] = {
            {"GUObjects_1", "91 E1 03 ? AA E0 03 08 AA E2 03 1F 2A", -7},
            {"GUObjects_2", "B4 21 0C 40 B9 ? ? ? ? ? ? ? 91", 5},
            {"GUObjects_3", "9F E5 00 ? 00 E3 FF ? 40 E3 ? ? A0 E1", -2},
            {"GUObjects_4", "96 df 02 17 ? ? ? ? 54 ? ? ? ? ? ? ? 91 e1 03 13 aa", 9},
            {"GUObjects_5", "f4 03 01 2a ? 00 00 34 ? ? ? ? ? ? ? ? ? ? 00 54 ? 00 00 14 ? ? ? ? ? ? ? 91", 0x18},
            {"GUObjects_6", "69 3e 40 b9 1f 01 09 6b ? ? ? 54 e1 03 13 aa ? ? ? ? f4 4f ? a9 ? ? ? ? ? ? ? 91", 0x18},
        };
        unsigned objectHits = 0;
        for (const auto &sig : objectSignatures)
        {
            const uintptr_t instruction = findIdaPattern(map_type, sig.pattern, sig.step);
            const uintptr_t global = Arm64::Decode_ADRP_ADD(instruction);
            const bool readable = global && kPtrValidator.isPtrReadable(reinterpret_cast<const void *>(global));
            std::printf("QA_DFM %-13s hit_rva=0x%llx candidate_rva=0x%llx readable=%d\n",
                        sig.label,
                        (unsigned long long)(instruction ? instruction - base : 0),
                        (unsigned long long)(global && global >= base ? global - base : 0),
                        readable ? 1 : 0);
            if (readable)
                ++objectHits;
        }

        const uintptr_t nameInsn = findIdaPattern(
            map_type, "91 ? 10 81 52 ? ? 21 8b", -7);
        const uintptr_t nameGlobal = Arm64::Decode_ADRP_ADD(nameInsn);
        const bool nameReadable =
            nameGlobal && kPtrValidator.isPtrReadable(reinterpret_cast<const void *>(nameGlobal));
        std::printf("QA_DFM NamePool      hit_rva=0x%llx candidate_rva=0x%llx readable=%d\n",
                    (unsigned long long)(nameInsn ? nameInsn - base : 0),
                    (unsigned long long)(nameGlobal && nameGlobal >= base ? nameGlobal - base : 0),
                    nameReadable ? 1 : 0);
        std::printf("QA_DFM SUMMARY readable_object_candidates=%u namepool_candidate=%d\n",
                    objectHits, nameReadable ? 1 : 0);
        return objectHits > 0 && nameReadable;
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