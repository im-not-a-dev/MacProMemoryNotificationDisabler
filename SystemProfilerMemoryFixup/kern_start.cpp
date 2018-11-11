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

// System Information binary path.
static const char *binPathSystemInformation = "/Applications/Utilities/System Information.app/Contents/MacOS/System Information";

// System Information libraries binary paths.
static const char *binPathAppleSystemInfo = "/System/Library/PrivateFrameworks/AppleSystemInfo.framework/Versions/A/AppleSystemInfo";
static const char *binPathSPMemoryReporter = "/System/Library/SystemProfiler/SPMemoryReporter.spreporter/Contents/MacOS/SPMemoryReporter";

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
    1
};

UserPatcher::BinaryModInfo ADDPR(binaryAirSystemInformation)[] {
    { binPathSystemInformation, &patchAir, 1 }
};
UserPatcher::BinaryModInfo ADDPR(binaryAirSPMemoryReporter)[] {
    { binPathSPMemoryReporter, &patchAir, 1 }
};
UserPatcher::BinaryModInfo ADDPR(binaryAirSystemInfo)[] {
    { binPathAppleSystemInfo, &patchAir, 1 }
};

// MacBookPro10 name patches.
static const uint8_t findPro10[] = "MacBookPro10";
static const uint8_t replacePro10[] = "MacBookXro10";
static UserPatcher::BinaryModPatch patchPro10 {
    CPU_TYPE_X86_64,
    findPro10,
    replacePro10,
    strlen(reinterpret_cast<const char *>(findPro10)),
    0,
    1,
    UserPatcher::FileSegment::SegmentTextCstring,
    1
};

UserPatcher::BinaryModInfo ADDPR(binaryPro10SystemInformation)[] {
    { binPathSystemInformation, &patchPro10, 1 }
};
UserPatcher::BinaryModInfo ADDPR(binaryPro10SPMemoryReporter)[] {
    { binPathSPMemoryReporter, &patchPro10, 1 }
};
UserPatcher::BinaryModInfo ADDPR(binaryPro10SystemInfo)[] {
    { binPathAppleSystemInfo, &patchPro10, 1 }
};

// System Information process info.
static UserPatcher::ProcInfo procInfo = { binPathSystemInformation, static_cast<uint32_t>(strlen(binPathSystemInformation)), 1 };

// Main function.
static void spmemfxStart() {
    // Do product name patches.
    lilu.onProcLoadForce(&procInfo, 1, nullptr, nullptr, ADDPR(binaryAirSystemInformation), 1);
    lilu.onProcLoadForce(&procInfo, 1, nullptr, nullptr, ADDPR(binaryAirSystemInfo), 1);
    lilu.onProcLoadForce(&procInfo, 1, nullptr, nullptr, ADDPR(binaryAirSPMemoryReporter), 1);
    lilu.onProcLoadForce(&procInfo, 1, nullptr, nullptr, ADDPR(binaryPro10SystemInformation), 1);
    lilu.onProcLoadForce(&procInfo, 1, nullptr, nullptr, ADDPR(binaryPro10SystemInfo), 1);
    lilu.onProcLoadForce(&procInfo, 1, nullptr, nullptr, ADDPR(binaryPro10SPMemoryReporter), 1);
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
    KernelVersion::Mojave,
    []() {
        spmemfxStart();
    }
};
