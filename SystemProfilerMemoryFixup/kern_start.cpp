/*
 * Copyright (c) 2018 John Davis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>
#include <Headers/kern_file.hpp>

// Pathes to binaries.
static const char *binPathSystemInformation = "/Applications/Utilities/System Information.app/Contents/MacOS/System Information";
static const char *binPathSystemInformationCatalina = "/System/Applications/Utilities/System Information.app/Contents/MacOS/System Information";
static const char *binPathSPMemoryReporter = "/System/Library/SystemProfiler/SPMemoryReporter.spreporter/Contents/MacOS/SPMemoryReporter";

static const uint32_t SectionActive = 1;

// MacBookAir name patches.
static const uint8_t findAir[] = "MacBookAir";
static const uint8_t replaceAir[] = "MacBookXir";
static UserPatcher::BinaryModPatch patchAir {
    CPU_TYPE_X86_64,
    findAir,
    replaceAir,
    strlen(reinterpret_cast<const char *>(findAir)),
    0,
    1,
    UserPatcher::FileSegment::SegmentTextCstring,
    SectionActive
};

// Find:    BF 02 00 00 00 E8 XX XX XX XX
// Replace: B8 08 00 00 00 0F 1F 44 00 00
static const size_t patchBytesCount = 10;
static const uint8_t replaceBytes[patchBytesCount] = { 0xB8, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x1F, 0x44, 0x00, 0x00 };

// Patching info for System Information binary.
static uint8_t findBytesSystemInformation[patchBytesCount] = { };
static UserPatcher::BinaryModPatch patchBytesSystemInformation {
    CPU_TYPE_X86_64,
    findBytesSystemInformation,
    replaceBytes,
    patchBytesCount,
    0,
    1,
    UserPatcher::FileSegment::SegmentTextText,
    SectionActive
};

// Patching info for SPMemoryReporter binary.
static uint8_t findBytesSPMemoryReporter[patchBytesCount] = { };
static UserPatcher::BinaryModPatch patchBytesSPMemoryReporter {
    CPU_TYPE_X86_64,
    findBytesSPMemoryReporter,
    replaceBytes,
    patchBytesCount,
    0,
    1,
    UserPatcher::FileSegment::SegmentTextText,
    SectionActive
};

// BinaryModInfo array containing all patches required. Paths changed in 10.15.
static UserPatcher::BinaryModInfo binaryPatchesCatalina[] {
    { binPathSystemInformationCatalina, &patchBytesSystemInformation, 1},
    { binPathSPMemoryReporter, &patchBytesSPMemoryReporter, 1},
    { binPathSystemInformationCatalina, &patchAir, 1 },
    { binPathSPMemoryReporter, &patchAir, 1 }
};

// BinaryModInfo array containing all patches required for 10.14 and below.
static UserPatcher::BinaryModInfo binaryPatches[] {
    { binPathSystemInformation, &patchBytesSystemInformation, 1},
    { binPathSPMemoryReporter, &patchBytesSPMemoryReporter, 1},
    { binPathSystemInformation, &patchAir, 1 },
    { binPathSPMemoryReporter, &patchAir, 1 }
};

// BinaryModInfo array containing all patches required for 10.8.
// 10.8 does not have ASI_IsPlatformFeatureEnabled or strings in SPMemoryReporter.
static UserPatcher::BinaryModInfo binaryPatchesML[] {
    { binPathSystemInformation, &patchAir, 1 },
};

// System Information process info.
static UserPatcher::ProcInfo procInfoCatalina = { binPathSystemInformationCatalina, static_cast<uint32_t>(strlen(binPathSystemInformationCatalina)), 1 };

// System Information process info.
static UserPatcher::ProcInfo procInfo = { binPathSystemInformation, static_cast<uint32_t>(strlen(binPathSystemInformation)), 1 };

static void buildPatch(KernelPatcher &patcher, const char *path, uint8_t *findBuffer) {
    DBGLOG("SystemProfilerMemoryFixup", "buildPatches() start");
    
    // Get contents of binary.
    size_t outSize;
    uint8_t *buffer = FileIO::readFileToBuffer(path, outSize);
    if (buffer == NULL)
        panic("SystemProfilerMemoryFixup: Failed to read binary: %s\n", path);
    
    // Find where ASI_IsPlatformFeatureEnabled is called.
    off_t index = 0;
    for (off_t i = 0; i < outSize; i++) {
        if (buffer[i] == 0xBF && buffer[i+1] == 0x02 && buffer[i+2] == 0x00
            && buffer[i+3] == 0x00 && buffer[i+4] == 0x00 && buffer[i+5] == 0xE8) {
            index = i;
            break;
        }
    }
    
    // If we found no match, we can't go on.
    if (index == 0)
        panic("SystemProfilerMemoryFixup: Failed to get index into binary: %s\n", path);
    
    // Build find pattern.
    uint8_t *bufferOffset = buffer + index;
    for (uint32_t i = 0; i < patchBytesCount; i++)
        findBuffer[i] = bufferOffset[i];
    
    // Free buffer.
    Buffer::deleter(buffer);
}

static void buildPatchesCatalina(void *user, KernelPatcher &patcher) {
    // Build patches for binaries.
    buildPatch(patcher, binPathSystemInformationCatalina, findBytesSystemInformation);
    buildPatch(patcher, binPathSPMemoryReporter, findBytesSPMemoryReporter);
}

static void buildPatches(void *user, KernelPatcher &patcher) {
    // Build patches for binaries.
    buildPatch(patcher, binPathSystemInformation, findBytesSystemInformation);
    buildPatch(patcher, binPathSPMemoryReporter, findBytesSPMemoryReporter);
}

// Main function.
static void spmemfxStart() {
    DBGLOG("SystemProfilerMemoryFixup", "start");
    
    // Are we on 10.15 or above?
    if (getKernelVersion() >= KernelVersion::Catalina) {
        // Load callback so we can determine patterns to search for.
        lilu.onPatcherLoad(buildPatchesCatalina);
        
        // Load patches into Lilu for 10.15+.
        lilu.onProcLoadForce(&procInfoCatalina, 1, nullptr, nullptr, binaryPatchesCatalina, arrsize(binaryPatchesCatalina));
    } else if (getKernelVersion() >= KernelVersion::Mavericks) {
        // Load callback so we can determine patterns to search for.
        lilu.onPatcherLoad(buildPatches);
        
        // Load patches into Lilu for 10.9 to 10.14.
        lilu.onProcLoadForce(&procInfo, 1, nullptr, nullptr, binaryPatches, arrsize(binaryPatches));
    } else if (getKernelVersion() == KernelVersion::MountainLion) {
        // 10.8 requires only a single patch.
        lilu.onProcLoadForce(&procInfo, 1, nullptr, nullptr, binaryPatchesML, arrsize(binaryPatchesML));
    }
}

// Boot args.
static const char *bootargOff[] {
    "-spmemfxoff"
};
static const char *bootargDebug[] {
    "-spmemfxdbg"
};
static const char *bootargBeta[] {
    "-spmemfxbeta"
};

// Plugin configuration.
PluginConfiguration ADDPR(config) {
    xStringify(PRODUCT_NAME),
    parseModuleVersion(xStringify(MODULE_VERSION)),
    LiluAPI::AllowNormal,
    bootargOff,
    arrsize(bootargOff),
    bootargDebug,
    arrsize(bootargDebug),
    bootargBeta,
    arrsize(bootargBeta),
    KernelVersion::MountainLion,
    KernelVersion::Catalina,
    []() {
        spmemfxStart();
    }
};
