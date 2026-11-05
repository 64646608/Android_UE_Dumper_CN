
#include <cerrno>
#include <cstdlib>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <unordered_map>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>

#include "Utils/KittyCmdln.hpp"
#include "Utils/Logger.hpp"
#include "Utils/ProgressUtils.hpp"

#include "Dumper.hpp"

#include "UE/UEMemory.hpp"
#include "UE/UEGameProfile.hpp"

#include "UE/UEGameProfiles/ArenaBreakout.hpp"
#include "UE/UEGameProfiles/DeltaForce.hpp"
#include "UE/UEGameProfiles/Farlight.hpp"
#include "UE/UEGameProfiles/Sfps2.hpp"
#include "UE/UEGameProfiles/Valorant.hpp"
#include "UE/UEGameProfiles/NRC.hpp"
#include "UE/UEGameProfiles/PUBG.hpp"
/*
#include "UE/UEGameProfiles/BlackClover.hpp"
#include "UE/UEGameProfiles/Dislyte.hpp"
#include "UE/UEGameProfiles/MortalKombat.hpp"
#include "UE/UEGameProfiles/PES.hpp"
#include "UE/UEGameProfiles/Torchlight.hpp"
#include "UE/UEGameProfiles/WutheringWaves.hpp"
#include "UE/UEGameProfiles/RealBoxing2.hpp"
#include "UE/UEGameProfiles/OdinValhalla.hpp"
#include "UE/UEGameProfiles/Injustice2.hpp"
#include "UE/UEGameProfiles/RooftopsParkour.hpp"
#include "UE/UEGameProfiles/BabyYellow.hpp"
#include "UE/UEGameProfiles/TowerFantasy.hpp"
#include "UE/UEGameProfiles/BladeSoul.hpp"
#include "UE/UEGameProfiles/Lineage2.hpp"
#include "UE/UEGameProfiles/NightCrows.hpp"
#include "UE/UEGameProfiles/Case2.hpp"
#include "UE/UEGameProfiles/KingArthur.hpp"
#include "UE/UEGameProfiles/Century.hpp"
#include "UE/UEGameProfiles/HelloNeighbor.hpp"
#include "UE/UEGameProfiles/HelloNeighborND.hpp"
#include "UE/UEGameProfiles/SFG2.hpp"
#include "UE/UEGameProfiles/ArkUltimate.hpp"
#include "UE/UEGameProfiles/Auroria.hpp"
#include "UE/UEGameProfiles/LineageW.hpp"
*/
std::vector<IGameProfile *> UE_Games = {
    new ArenaBreakoutProfile(),
    new DeltaForceProfile(),
    new FarlightProfile(),
    new ShuishaProfile(),
    new ValorantProfile(),
    new NRCProfile(),
    new PUBGProfile(),
    /*
    new PESProfile(),
    new DislyteProfile(),
    new MortalKombatProfile(),

    new TorchlightProfile(),

    new BlackCloverProfile(),
    new WutheringWavesProfile(),
    new RealBoxing2Profile(),
    new OdinValhallaProfile(),
    new Injustice2Profile(),

    new RooftopParkourProfile(),
    new BabyYellowProfile(),
    new TowerFantasyProfile(),
    new BladeSoulProfile(),
    new Lineage2Profile(),
    new Case2Profile(),
    new CenturyProfile(),
    new KingArthurProfile(),
    new NightCrowsProfile(),
    new HelloNeighborProfile(),
    new HelloNeighborNDProfile(),
    new SFG2Profile(),
    new ArkUltimateProfile(),
    new AuroriaProfile(),
    new LineageWProfile(),
    */
};

#define kUEDUMPER_VERSION "4.1.2"

bool bNeededHelp = false;

int main(int argc, char **args)
{
    setbuf(stdout, nullptr);
    setbuf(stderr, nullptr);
    setbuf(stdin, nullptr);

    LOGI("Using UE Dumper %s", kUEDUMPER_VERSION);
    LOGI("original author:MJx0 https://github.com/MJx0");
    LOGI("Secondary creations from 曦曦(DreamFekk) https://github.com/DreamFekk");
    LOGI("Thanks for my friend ZRO https://github.com/bikhx's help!!");

    KittyCmdln cmdline(argc, args);

    cmdline.setUsage("Usage: ./UEDump3r [-h] [-o] [ options ]");

    cmdline.addCmd("-h", "--help", "show available arguments", false, [&cmdline]()
    { std::cout << cmdline.toString() << std::endl; bNeededHelp = true; });

    char sOutDir[0xff] = {0};
    cmdline.addScanf("-o", "--output", "specify output directory path.", false, "%s", sOutDir);

    // optional
    char sGamePkg[0xff] = {0};
    cmdline.addScanf("-p", "--package", "specify game package ID in advance.", false, "%s", sGamePkg);

    // options
    bool bDumpLib = false;
    cmdline.addFlag("-d", "--dumplib", "dump UE library from memory.", false, &bDumpLib);

    int requestedPID = 0;
    cmdline.addScanf("-P", "--pid", "select a specific PID without an interactive prompt.", false, "%d", &requestedPID);

    // Optional hexadecimal RVAs. Both must be provided together.
    char namePoolArg[0x40] = {0};
    char objectsArg[0x40] = {0};
    cmdline.addScanf("-N", "--namepool-rva", "NamePool RVA relative to libUE4.so, e.g. 0x200F7D00.", false, "%s", namePoolArg);
    cmdline.addScanf("-G", "--guobject-rva", "GUObjectArray RVA relative to libUE4.so, e.g. 0x2011EB48.", false, "%s", objectsArg);

    bool bQaDfmDump = false;
    cmdline.addFlag("-Q", "--qa-dfm-dump", "opt-in real SDK dump using prevalidated DeltaForce runtime offsets.", false, &bQaDfmDump);

    bool bQaDfmDiagnostic = false;
    cmdline.addFlag("-q", "--qa-dfm-diagnose", "read-only DeltaForce runtime signature diagnostics (no SDK dump).", false, &bQaDfmDiagnostic);

    cmdline.parseArgs();

    if (bNeededHelp)
    {
        return 0;
    }

    if (!cmdline.requiredCmdsCheck())
    {
        LOGE("Required arguments needed. see -h.");
        return 1;
    }

    std::string sOutDirectory = sOutDir, sGamePackage = sGamePkg;
    uintptr_t namePoolRVA = 0x20291EC0;
    uintptr_t objectsRVA = 0x202B8CF8;
    const bool customName = namePoolArg[0] != '\0';
    const bool customObjects = objectsArg[0] != '\0';
    if (customName != customObjects)
    {
        LOGE("--namepool-rva and --guobject-rva must be supplied together.");
        return 2;
    }
    if ((customName || customObjects) && !bQaDfmDump)
    {
        LOGE("Custom RVAs require --qa-dfm-dump.");
        return 2;
    }
    auto parseRVA = [](const char *value, uintptr_t &target) -> bool
    {
        if (!value || !*value || *value == '-') return false;
        char *end = nullptr;
        errno = 0;
        unsigned long long n = std::strtoull(value, &end, 0);
        if (errno != 0 || end == value || *end != '\0' || n == 0 ||
            n >= 0x100000000ULL) return false;
        target = static_cast<uintptr_t>(n);
        return true;
    };
    if (customName && (!parseRVA(namePoolArg, namePoolRVA) ||
                       !parseRVA(objectsArg, objectsRVA)))
    {
        LOGE("Invalid RVA. Supply both offsets in 0x-prefixed hexadecimal.");
        return 2;
    }
    if (bQaDfmDump && bQaDfmDiagnostic)
    {
        LOGE("Choose exactly one: --qa-dfm-dump OR --qa-dfm-diagnose.");
        return 2;
    }
    if (bQaDfmDump && sGamePackage != "com.tencent.tmgp.dfm")
    {
        LOGE("--qa-dfm-dump requires -p com.tencent.tmgp.dfm.");
        return 2;
    }

    if (sOutDirectory.empty() && !bQaDfmDiagnostic)
    {
        LOGE("Output directory path is not specified.");
        return 1;
    }

    if (sGamePackage.empty())
    {
        std::sort(UE_Games.begin(), UE_Games.end(), [](const IGameProfile *a, const IGameProfile *b)
        {
            return a->GetAppName() < b->GetAppName();
        });

        std::cout << "Choose from the available games:" << std::endl;
        int gameIndex = 1;
        std::map<int, std::pair<int, int>> gameIndexMap;
        for (size_t i = 0; i < UE_Games.size(); i++)
        {
            const auto &appIDs = UE_Games[i]->GetAppIDs();
            for (size_t j = 0; j < appIDs.size(); j++)
            {
                const char *nspace = gameIndex < 10 ? "  " : " ";
                std::cout << "\t" << gameIndex << nspace << ": " << UE_Games[i]->GetAppName() << " | " << appIDs[j].c_str() << std::endl;
                gameIndexMap[gameIndex] = {i, j};
                gameIndex++;
            }
        }

        std::cout << "Game number: ";
        int gameNumber = 0;
        scanf("%d", &gameNumber);
        if (gameIndexMap.count(gameNumber) <= 0)
        {
            LOGE("Game number is not available.");
            return 1;
        }

        sGamePackage = UE_Games[gameIndexMap[gameNumber].first]->GetAppIDs()[gameIndexMap[gameNumber].second];
    }


    auto gamePIDs = KittyMemoryEx::getPIDsOf(sGamePackage);
    if (gamePIDs.empty())
    {
        LOGE("Couldn't find \"%s\" in the running processes list.", sGamePackage.c_str());
        return 1;
    }

    // Prefer a process that actually has the UE runtime mapped. A shell or
    // su process containing the package name in its command line can also be
    // returned by process-name substring discovery; blindly asking for
    // "process number" is inappropriate for non-interactive QA diagnostics.
    auto hasUE4Map = [](pid_t pid) -> bool
    {
        return !KittyMemoryEx::getMapsContain(pid, "libUE4.so").empty() ||
               !KittyMemoryEx::getMapsContain(pid, "libUnreal.so").empty();
    };
    auto hasExactCmdline = [&sGamePackage](pid_t pid) -> bool
    {
        char path[64] = {0};
        std::snprintf(path, sizeof(path), "/proc/%d/cmdline", int(pid));
        FILE *fp = std::fopen(path, "rb");
        if (!fp) return false;
        char name[512] = {0};
        const size_t length = std::fread(name, 1, sizeof(name) - 1, fp);
        std::fclose(fp);
        return length && sGamePackage == std::string(name);
    };

    pid_t gamePID = 0;
    if (requestedPID > 0)
    {
        bool found = false;
        for (pid_t p : gamePIDs)
            if (p == requestedPID) found = true;
        if (!found || !hasUE4Map(requestedPID))
        {
            LOGE("Requested --pid %d is not a recognized game process with a loaded UE library.", requestedPID);
            return 2;
        }
        gamePID = requestedPID;
    }
    else if ((bQaDfmDiagnostic || bQaDfmDump) && gamePIDs.size() > 1)
    {
        std::vector<pid_t> mapped;
        std::vector<pid_t> exact;
        for (pid_t pid : gamePIDs)
        {
            if (!hasUE4Map(pid)) continue;
            mapped.push_back(pid);
            if (hasExactCmdline(pid)) exact.push_back(pid);
        }
        if (exact.size() == 1)
            gamePID = exact.front();
        else if (mapped.size() == 1)
            gamePID = mapped.front();
        else
        {
            LOGE("QA diagnostics found %zu possible game processes with UE mappings; cannot choose safely.", mapped.size());
            for (pid_t pid : mapped)
                LOGI("Mapped UE process: PID %d (exact_cmdline=%d)", int(pid), hasExactCmdline(pid) ? 1 : 0);
            LOGE("Specify --pid <PID> to select one without interaction.");
            return 2;
        }
        LOGI("QA automatically selected PID %d (UE library mapped; no interactive prompt).", int(gamePID));
    }
    else if (gamePIDs.size() > 1)
    {
        std::cout << "Found multiple processes with same package name, Choose one: " << std::endl;
        for (size_t i = 0; i < gamePIDs.size(); i++)
            std::cout << "\t" << i << ": PID(" << gamePIDs[i] << ")" << std::endl;
        std::cout << "Process number: " << std::flush;
        int gamePidIndex = -1;
        if (scanf("%d", &gamePidIndex) != 1 ||
            gamePidIndex < 0 || size_t(gamePidIndex) >= gamePIDs.size())
        {
            LOGE("No valid process index selected.");
            return 1;
        }
        gamePID = gamePIDs[gamePidIndex];
    }
    else
    {
        gamePID = gamePIDs.front();
    }

    LOGI("Game: %s", sGamePackage.c_str());
    LOGI("Process ID: %d", gamePID);
    LOGI("Output directory: %s", sOutDirectory.c_str());
    LOGI("Dump Library: %s", bDumpLib ? "true" : "false");
    LOGI("==========================");

    LOGI("Initializing Memory...");
    if (!kMgr.initialize(gamePID, EK_MEM_OP_SYSCALL, false) && !kMgr.initialize(gamePID, EK_MEM_OP_IO, false))
    {
        LOGE("Failed to initialize KittyMemoryMgr.");
        return 1;
    }

    if (bQaDfmDiagnostic)
    {
        if (sGamePackage != "com.tencent.tmgp.dfm")
        {
            LOGE("QA signature diagnostic is only available for the DeltaForce profile.");
            return 2;
        }
        kPtrValidator.setPID(gamePID);
        kPtrValidator.setUseCache(true);
        kPtrValidator.refreshRegionCache();
        DeltaForceProfile profile;
        const bool matched = profile.PrintQAPatternDiagnostics();
        return matched ? 0 : 2;
    }

    std::string sDumpDir = sOutDirectory + "/UEDump3r";
    std::string sDumpGameDir = sDumpDir + "/" + sGamePackage;
    IOUtils::delete_directory(sDumpGameDir);

    if (IOUtils::mkdir_recursive(sDumpGameDir, 0777) == -1)
    {
        int err = errno;
        LOGE("Couldn't create Output Directory [\"%s\"] error=%d | %s.", sDumpDir.c_str(), err, strerror(err));
        return 1;
    }

    UEDumper uEDumper{};

    uEDumper.setDumpExeInfoNotify([](bool bFinished)
    {
        if (!bFinished)
        {
            LOGI("Dumping Executable Info...");
        }
    });

    uEDumper.setDumpNamesInfoNotify([](bool bFinished)
    {
        if (!bFinished)
        {
            LOGI("Dumping Names Info...");
        }
    });

    uEDumper.setDumpObjectsInfoNotify([](bool bFinished)
    {
        if (!bFinished)
        {
            LOGI("Dumping Objects Info...");
        }
    });

    uEDumper.setDumpOffsetsInfoNotify([](bool bFinished)
    {
        if (!bFinished)
        {
            LOGI("Dumping Offsets Info...");
        }
    });

    uEDumper.setObjectsProgressCallback([](const SimpleProgressBar &progress)
    {
        static bool once = false;
        if (!once)
        {
            once = true;
            LOGI("Gathering UObjects....");
        };
        static int lastPercent = -1;
        int currPercent = progress.getPercentage();
        if (lastPercent != currPercent)
        {
            lastPercent = currPercent;
            progress.print();
            if (progress.isComplete())
                std::cout << "\n";
        }
    });

    uEDumper.setDumpProgressCallback([](const SimpleProgressBar &progress)
    {
        static bool once = false;
        if (!once)
        {
            once = true;
            LOGI("Dumping....");
        };
        static int lastPercent = -1;
        int currPercent = progress.getPercentage();
        if (lastPercent != currPercent)
        {
            lastPercent = currPercent;
            progress.print();
            if (progress.isComplete())
                std::cout << "\n";
        }
    });

    bool dumpSuccess = false;
    std::unordered_map<std::string, BufferFmt> dumpbuffersMap;
    auto dmpStart = std::chrono::steady_clock::now();

    for (auto &it : UE_Games)
    {
        for (auto &pkg : it->GetAppIDs())
        {
            if (sGamePackage != pkg)
                continue;

            if (bQaDfmDump)
            {
                LOGI("QA_DFM REAL_DUMP enabled. Runtime offsets must pass NamePool and GUObject validation before dump.");
                LOGI("QA_DFM runtime offsets: NamePool=0x%llx GUObjectArray=0x%llx (%s)",
                     (unsigned long long)namePoolRVA,
                     (unsigned long long)objectsRVA,
                     customName ? "custom" : "default");
                static_cast<DeltaForceProfile *>(it)->EnableQARuntimeDump(true, namePoolRVA, objectsRVA);
            }

            if (bDumpLib)
            {
                auto ue_elf = it->GetUnrealELF();
                if (!ue_elf.isValid())
                {
                    LOGE("Couldn't find a valid UE ELF in target process maps.");
                    return 1;
                }

                LOGI("Dumping unreal lib from memory...");
                std::string libDumpPath = KittyUtils::String::Fmt("%s/libUE_%p-%p.so", sDumpGameDir.c_str(), ue_elf.base(), ue_elf.end());
                bool res = kMgr.dumpMemELF(ue_elf, libDumpPath);
                LOGI("Dumping lib: %s.", res ? "success" : "failed");
                if (res)
                {
                    LOGI("%s", libDumpPath.c_str());
                }
                LOGI("==========================");
            }

            LOGI("Initializing Dumper...");
            if (uEDumper.Init(it))
            {
                dumpSuccess = uEDumper.Dump(&dumpbuffersMap);
            }

            goto done;
        }
    }

done:

    if (!dumpSuccess && uEDumper.GetLastError().empty())
    {
        LOGE("Game is not supported. check AppID.");
        return 1;
    }

    if (dumpbuffersMap.empty())
    {
        LOGE("Dump Failed, Error <Buffers empty>");
        LOGE("Dump Status <%s>", uEDumper.GetLastError().c_str());
        return 1;
    }

    LOGI("Saving Files...");

    for (const auto &it : dumpbuffersMap)
    {
        if (!it.first.empty())
        {
            std::string path = KittyUtils::String::Fmt("%s/%s", sDumpGameDir.c_str(), it.first.c_str());
            it.second.writeBufferToFile(path);
        }
    }

    auto dmpEnd = std::chrono::steady_clock::now();
    std::chrono::duration<float, std::milli> dmpDurationMS = (dmpEnd - dmpStart);

    if (!uEDumper.GetLastError().empty())
    {
        LOGI("Dump Status: %s", uEDumper.GetLastError().c_str());
    }
    LOGI("Dump Duration: %.2fms", dmpDurationMS.count());
    LOGI("Dump Location: %s", sDumpGameDir.c_str());

    return 0;
}