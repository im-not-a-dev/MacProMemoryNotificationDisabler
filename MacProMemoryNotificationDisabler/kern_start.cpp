/*
 * Copyright (c) 2018-2019 John Davis
 * Copyright (c) 2020 IOIIIO
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

// Paths to binaries.
static const char *binPathMemorySlotNotification = "/System/Library/CoreServices/MemorySlotNotification";
static const char *binPathSystemInformationCatalina = "/System/Applications/Utilities/System Information.app/Contents/MacOS/System Information";

static const uint32_t SectionActive = 1;

static const size_t patchBytesCountTooMuchSI = 5;
static uint8_t findBytesTooMuchSI[patchBytesCountTooMuchSI] = { 0x84, 0xC0, 0x74, 0x64, 0x48 };
static const uint8_t replaceBytesTooMuchSI[patchBytesCountTooMuchSI] = { 0x84, 0xC0, 0x75, 0x64, 0x48 };
static UserPatcher::BinaryModPatch patchTooMuchSI {
    CPU_TYPE_X86_64,
    0,
    findBytesTooMuchSI,
    replaceBytesTooMuchSI,
    patchBytesCountTooMuchSI,
    0,
    1,
    UserPatcher::FileSegment::SegmentTextText,
    SectionActive
};

/*
 Find:    74 xx 4C 89 F7 E8 xx xx xx xx
 Replace: 90 90 4C 89 F7 90 90 90 90 90
 */
static const size_t patchBytesCount = 10;
static const uint8_t replaceBytes[patchBytesCount] = { 0x90, 0x90, 0x4C, 0x89, 0xF7, 0x90, 0x90, 0x90, 0x90, 0x90 };

// Patching info for MemorySlotNotification binary.
static uint8_t findBytesMemorySlotNotification[patchBytesCount] = { };
static UserPatcher::BinaryModPatch patchBytesMemorySlotNotification {
    CPU_TYPE_X86_64,
    0,
    findBytesMemorySlotNotification,
    replaceBytes,
    patchBytesCount,
    0,
    1,
    UserPatcher::FileSegment::SegmentTextText,
    SectionActive
};

// BinaryModInfo array containing all patches required. 
static UserPatcher::BinaryModInfo binaryPatchesCatalina[] {
    { binPathMemorySlotNotification, &patchBytesMemorySlotNotification, 1},
    { binPathSystemInformationCatalina, &patchTooMuchSI, 1 },
};

// System Information process info.
static UserPatcher::ProcInfo procInfoCatalina = { binPathMemorySlotNotification, static_cast<uint32_t>(strlen(binPathMemorySlotNotification)), 1 };

static void buildPatch(KernelPatcher &patcher, const char *path, uint8_t *findBuffer) {
    DBGLOG("MacProMemoryNotificationDisabler", "buildPatches() start");
    
    // Get contents of binary.
    size_t outSize;
    uint8_t *buffer = FileIO::readFileToBuffer(path, outSize);
    if (buffer == NULL) {
        DBGLOG("MacProMemoryNotificationDisabler", "Failed to read binary: %s\n", path);
        procInfoCatalina.section = procInfoCatalina.SectionDisabled;
        return;
    }
    
    // Find where the notification is called.
    off_t index = 0;
    for (off_t i = 0; i < outSize; i++) {
        if (buffer[i] == 0x74 && buffer[i+2] == 0x4C && buffer[i+3] == 0x89 
            && buffer[i+4] == 0xF7 && buffer[i+5] == 0xE8) {
            index = i;
            break;
        }
    }
    
    // If we found no match, we can't go on.
    if (index == 0) {
        DBGLOG("MacProMemoryNotificationDisabler", "Failed to get index into binary: %s\n", path);
        Buffer::deleter(buffer);
        procInfoCatalina.section = procInfoCatalina.SectionDisabled;
        return;
    }
        
    // Build find pattern.
    uint8_t *bufferOffset = buffer + index;
    for (uint32_t i = 0; i < patchBytesCount; i++)
        findBuffer[i] = bufferOffset[i];
    
    // Free buffer.
    Buffer::deleter(buffer);
}

static void buildPatchesCatalina(void *user, KernelPatcher &patcher) {
    // Build patches for binaries.
    buildPatch(patcher, binPathMemorySlotNotification, findBytesMemorySlotNotification);
}

// Main function.
static void mpmndStart() {
    DBGLOG("MacProMemoryNotificationDisabler", "start");
    
    // Are we on 10.15 or above?
    if (getKernelVersion() >= KernelVersion::Catalina) {
        // Load callback so we can determine patterns to search for.
        lilu.onPatcherLoad(buildPatchesCatalina);
        
        // Load patches into Lilu for 10.15+.
        lilu.onProcLoadForce(&procInfoCatalina, 1, nullptr, nullptr, binaryPatchesCatalina, arrsize(binaryPatchesCatalina));
    }
}

// Boot args.
static const char *bootargOff[] {
    "-mpmndoff"
};
static const char *bootargDebug[] {
    "-mpmnddbg"
};
static const char *bootargBeta[] {
    "-mpmndbeta"
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
    KernelVersion::Catalina,
    KernelVersion::BigSur,
    []() {
        mpmndStart();
    }
};
