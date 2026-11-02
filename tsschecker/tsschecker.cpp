//
//  tsschecker.cpp
//  tsschecker
//
//  Created by tihmstar on 28.02.23.
//

#include <tsschecker/tsschecker.hpp>
#include <tsschecker/TSSException.hpp>
#include <tsschecker/FirmwareAPI_IPSWME.hpp>

#include <libgeneral/macros.h>
#include <libgeneral/Utils.hpp>
#include <libirecovery.h>
#include <libfragmentzip/libfragmentzip.h>
#include <curl/curl.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

using namespace tihmstar;

#pragma mark database
struct bbdevice{
    const char *deviceModel;
    uint64_t bbgcid;
    size_t bbsnumSize;
};
// iPhone & iPod touch (1st generations) dont't have signing technology.
// BbGoldCertId and BbSNUM length per device, synced with 1Conan/tsschecker
static struct bbdevice bbdevices[] = {
    // Apple Silicon Macs
    {"ADP3,1", 0, 0},          // DTK (2020)
    {"ADP3,2", 0, 0},          // DTK (2020)
    {"iMac21,1", 0, 0},        // iMac (24-inch, M1, 2021)
    {"iMac21,2", 0, 0},        // iMac (24-inch, M1, 2021)
    {"Macmini9,1", 0, 0},      // Mac Mini (M1, 2020)
    {"MacBookAir10,1", 0, 0},  // MacBook Air (M1, 2020)
    {"MacBookPro17,1", 0, 0},  // MacBook Pro (13-inch, M1, 2020)
    {"MacBookPro18,1", 0, 0},  // MacBook Pro (M1 Pro, 16-inch, 2021)
    {"MacBookPro18,2", 0, 0},  // MacBook Pro (M1 Max, 16-inch, 2021)
    {"MacBookPro18,3", 0, 0},  // MacBook Pro (M1 Pro, 14-inch, 2021)
    {"MacBookPro18,4", 0, 0},  // MacBook Pro (M1 Max, 14-inch, 2021)
    {"Mac13,1", 0, 0},         // Mac Studio (M1 Max, 2022)
    {"Mac13,2", 0, 0},         // Mac Studio (M1 Ultra, 2022)
    {"Mac14,2", 0, 0},         // MacBook Air (M2, 2022)
    {"Mac14,7", 0, 0},         // MacBook Pro (13-inch, M2, 2022)
    {"Mac14,3", 0, 0},         // Mac mini (M2, 2023)
    {"Mac14,5", 0, 0},         // MacBook Pro (14-inch, M2 Max, 2023)
    {"Mac14,6", 0, 0},         // MacBook Pro (16-inch, M2 Max, 2023)
    {"Mac14,8", 0, 0},         // Mac Pro (M2 Ultra, 2023)
    {"Mac14,9", 0, 0},         // MacBook Pro (14-inch, M2 Pro, 2023)
    {"Mac14,10", 0, 0},        // MacBook Pro (16-inch, M2 Pro, 2023)
    {"Mac14,12", 0, 0},        // Mac mini (M2 Pro, 2023)
    {"Mac14,13", 0, 0},        // Mac Studio (M2 Max, 2023)
    {"Mac14,14", 0, 0},        // Mac Studio (M2 Ultra, 2023)
    {"Mac14,15", 0, 0},        // MacBook Air (15-inch, M2, 2023)
    {"Mac15,3", 0, 0},         // MacBook Pro (14-inch, M3, Nov 2023)
    {"Mac15,4", 0, 0},         // iMac 24-inch (M3, Two Ports, 2023)
    {"Mac15,5", 0, 0},         // iMac 24-inch (M3, Four Ports, 2023)
    {"Mac15,6", 0, 0},         // MacBook Pro (14-inch, M3 Pro, Nov 2023)
    {"Mac15,7", 0, 0},         // MacBook Pro (16-inch, M3 Pro, Nov 2023)
    {"Mac15,8", 0, 0},         // MacBook Pro (14-inch, M3 Max, Nov 2023)
    {"Mac15,9", 0, 0},         // MacBook Pro (16-inch, M3 Max, Nov 2023)
    {"Mac15,10", 0, 0},        // MacBook Pro (14-inch, M3 Max, Nov 2023)
    {"Mac15,11", 0, 0},        // MacBook Pro (16-inch, M3 Max, Nov 2023)
    {"Mac15,12", 0, 0},        // MacBook Air (13-inch, M3, 2024)
    {"Mac15,13", 0, 0},        // MacBook Air (15-inch, M3, 2024)
    {"Mac15,14", 0, 0},        // Mac Studio (M3 Ultra, 2025)
    {"Mac16,1", 0, 0},         // MacBook Pro (14-inch, M4, Nov 2024)
    {"Mac16,2", 0, 0},         // iMac 24-inch (M4, Two Ports, 2024)
    {"Mac16,3", 0, 0},         // iMac 24-inch (M4, Four Ports, 2024)
    {"Mac16,5", 0, 0},         // MacBook Pro (16-inch, M4 Pro, Nov 2024)
    {"Mac16,6", 0, 0},         // MacBook Pro (14-inch, M4 Pro, Nov 2024)
    {"Mac16,7", 0, 0},         // MacBook Pro (16-inch, M4 Pro, Nov 2024)
    {"Mac16,8", 0, 0},         // MacBook Pro (14-inch, M4 Pro, Nov 2024)
    {"Mac16,9", 0, 0},         // Mac Studio (M4 Max, 2025)
    {"Mac16,10", 0, 0},        // Mac mini (M4, 2024)
    {"Mac16,11", 0, 0},        // Mac mini (M4 Pro, 2024)
    {"Mac16,12", 0, 0},        // MacBook Air (13-inch, M4, 2025)
    {"Mac16,13", 0, 0},        // MacBook Air (15-inch, M4, 2025)

    // Apple Displays
    {"AppleDisplay2,1", 0, 0}, // Studio Display

    // Apple T2 Coprocessor
    {"iBridge2,1",  0, 0}, // Apple T2 iMacPro1,1 (j137)
    {"iBridge2,3",  0, 0}, // Apple T2 MacBookPro15,1 (j680)
    {"iBridge2,4",  0, 0}, // Apple T2 MacBookPro15,2 (j132)
    {"iBridge2,5",  0, 0}, // Apple T2 Macmini8,1 (j174)
    {"iBridge2,6",  0, 0}, // Apple T2 MacPro7,1 (j160)
    {"iBridge2,7",  0, 0}, // Apple T2 MacBookPro15,3 (j780)
    {"iBridge2,8",  0, 0}, // Apple T2 MacBookAir8,1 (j140k)
    {"iBridge2,10", 0, 0}, // Apple T2 MacBookPro15,4 (j213)
    {"iBridge2,12", 0, 0}, // Apple T2 MacBookAir8,2 (j140a)
    {"iBridge2,14", 0, 0}, // Apple T2 MacBookPro16,1 (j152f)
    {"iBridge2,15", 0, 0}, // Apple T2 MacBookAir9,1 (j230k)
    {"iBridge2,16", 0, 0}, // Apple T2 MacBookPro16,2 (j214k)
    {"iBridge2,19", 0, 0}, // Apple T2 iMac20,1 (j185)
    {"iBridge2,20", 0, 0}, // Apple T2 iMac20,2 (j185f)
    {"iBridge2,21", 0, 0}, // Apple T2 MacBookPro16,3 (j223)
    {"iBridge2,22", 0, 0}, // Apple T2 MacBookPro16,4 (j215)

    // iPod touches
    {"iPod2,1", 0, 0}, // 2nd gen
    {"iPod3,1", 0, 0}, // 3rd gen
    {"iPod4,1", 0, 0}, // 4th gen
    {"iPod5,1", 0, 0}, // 5th gen
    {"iPod7,1", 0, 0}, // 6th gen
    {"iPod9,1", 0, 0}, // 7th gen

    // iPhones
    {"iPhone1,2",  0, 0},            // iPhone 3G
    {"iPhone2,1",  0, 0},            // iPhone 3GS
    {"iPhone3,1",  257, 12},         // iPhone 4 GSM
    {"iPhone3,2",  257, 12},         // iPhone 4 GSM (2012, Rev A)
    {"iPhone3,3",  2, 4},            // iPhone 4 CDMA
    {"iPhone4,1",  2, 4},            // iPhone 4s
    {"iPhone5,1",  3255536192, 4},   // iPhone 5 (GSM)
    {"iPhone5,2",  3255536192, 4},   // iPhone 5 (Global)
    {"iPhone5,3",  3554301762, 4},   // iPhone 5c (GSM)
    {"iPhone5,4",  3554301762, 4},   // iPhone 5c (Global)
    {"iPhone6,1",  3554301762, 4},   // iPhone 5s (GSM)
    {"iPhone6,2",  3554301762, 4},   // iPhone 5s (Global)
    {"iPhone7,1",  3840149528, 4},   // iPhone 6 Plus
    {"iPhone7,2",  3840149528, 4},   // iPhone 6
    {"iPhone8,1",  3840149528, 4},   // iPhone 6s
    {"iPhone8,2",  3840149528, 4},   // iPhone 6s Plus
    {"iPhone8,4",  3840149528, 4},   // iPhone SE (1st gen)
    {"iPhone9,1",  2315222105, 4},   // iPhone 7 (Global)
    {"iPhone9,2",  2315222105, 4},   // iPhone 7 Plus (Global)
    {"iPhone9,3",  1421084145, 12},  // iPhone 7 GSM
    {"iPhone9,4",  1421084145, 12},  // iPhone 7 Plus (GSM)
    {"iPhone10,1", 2315222105, 4},   // iPhone 8 (Global)
    {"iPhone10,2", 2315222105, 4},   // iPhone 8 Plus (Global)
    {"iPhone10,3", 2315222105, 4},   // iPhone X (Global)
    {"iPhone10,4", 524245983, 12},   // iPhone 8 (GSM)
    {"iPhone10,5", 524245983, 12},   // iPhone 8 Plus (GSM)
    {"iPhone10,6", 524245983, 12},   // iPhone X GSM
    {"iPhone11,2", 165673526, 12},   // iPhone XS
    {"iPhone11,4", 165673526, 12},   // iPhone XS Max (China)
    {"iPhone11,6", 165673526, 12},   // iPhone XS Max (Global)
    {"iPhone11,8", 165673526, 12},   // iPhone XR
    {"iPhone12,1", 524245983, 12},   // iPhone 11
    {"iPhone12,3", 524245983, 12},   // iPhone 11 Pro
    {"iPhone12,5", 524245983, 12},   // iPhone 11 Pro Max
    {"iPhone12,8", 524245983, 12},   // iPhone SE (2nd gen)
    {"iPhone13,1", 3095201109, 4},   // iPhone 12 mini
    {"iPhone13,2", 3095201109, 4},   // iPhone 12
    {"iPhone13,3", 3095201109, 4},   // iPhone 12 Pro
    {"iPhone13,4", 3095201109, 4},   // iPhone 12 Pro Max
    {"iPhone14,2", 495958265, 4},    // iPhone 13 Pro Max
    {"iPhone14,3", 495958265, 4},    // iPhone 13 Pro
    {"iPhone14,4", 495958265, 4},    // iPhone 13 mini
    {"iPhone14,5", 495958265, 4},    // iPhone 13
    {"iPhone14,6", 2241363181, 4},   // iPhone SE (3rd gen)
    {"iPhone14,7", 3559316616, 4},   // iPhone 14
    {"iPhone14,8", 3559316616, 4},   // iPhone 14 Plus
    {"iPhone15,2", 3559316616, 4},   // iPhone 14 Pro
    {"iPhone15,3", 3559316616, 4},   // iPhone 14 Pro Max
    {"iPhone15,4", 3452763205, 4},   // iPhone 15
    {"iPhone15,5", 3452763205, 4},   // iPhone 15 Plus
    {"iPhone16,1", 3452763205, 4},   // iPhone 15 Pro
    {"iPhone16,2", 3452763205, 4},   // iPhone 15 Pro Max
    {"iPhone17,1", 1652214800, 4},   // iPhone 16 Pro
    {"iPhone17,2", 1652214800, 4},   // iPhone 16 Pro Max
    {"iPhone17,3", 1652214800, 4},   // iPhone 16
    {"iPhone17,4", 1652214800, 4},   // iPhone 16 Plus
    {"iPhone17,5", 4, 0},            // iPhone 16e
    {"iPhone18,3", 694094470, 8},    // iPhone 17
    {"iPhone18,1", 694094470, 8},    // iPhone 17 Pro
    {"iPhone18,2", 694094470, 8},    // iPhone 17 Pro Max
    {"iPhone18,4", 4, 0},            // iPhone Air
    //

    // iPads
    {"iPad1,1",    0, 0},            // iPad (1st gen)
    {"iPad2,1",    0, 0},            // iPad 2 Wi-Fi
    {"iPad2,2",    257, 12},         // iPad 2 GSM
    {"iPad2,3",    2, 4},            // iPad 2 CDMA
    {"iPad2,4",    0, 0},            // iPad 2 Wi-Fi (2012, Rev A)
    {"iPad3,1",    0, 0},            // iPad (3rd gen, Wi-Fi)
    {"iPad3,2",    4, 4},            // iPad (3rd gen, CDMA)
    {"iPad3,3",    4, 4},            // iPad (3rd gen, GSM)
    {"iPad3,4",    0, 0},            // iPad with Retina display (4th gen, Wi-Fi)
    {"iPad3,5",    3255536192, 4},   // iPad with Retina display (4th gen, CDMA)
    {"iPad3,6",    3255536192, 4},   // iPad with Retina display (4th gen, GSM)
    {"iPad6,11",   0, 0},            // iPad (5th gen, 2017, Wi-Fi)
    {"iPad6,12",   3840149528, 4},   // iPad (5th gen, 2017, Cellular)
    {"iPad7,5",    0, 0},            // iPad (6th gen, 2018, Wi-Fi)
    {"iPad7,6",    3840149528, 4},   // iPad (6th gen, 2018, Cellular)
    {"iPad7,11",   0, 0},            // iPad (7th gen, 2019, Wi-Fi)
    {"iPad7,12",   165673526, 12},   // iPad (7th gen, 2019, Cellular)
    {"iPad11,6",   0, 0},            // iPad (8th gen, 2020, Wi-Fi)
    {"iPad11,7",   165673526, 12},   // iPad (8th gen, 2020, Cellular)
    {"iPad12,1",   0, 0},            // iPad (9th gen, 2021, Wi-Fi)
    {"iPad12,2",   165673526, 12},   // iPad (9th gen, 2021, Cellular)
    {"iPad13,18",  0, 0},            // iPad (10th gen, 2022, Wi-Fi)
    {"iPad13,19",  495958265, 4},    // iPad (10th gen, 2022, Cellular)
    {"iPad15,7",   0, 0},            // iPad (A16, 2025, Wi-Fi)
    {"iPad15,8",   3452763205, 4},   // iPad (A16, 2025, Cellular)
    
    // iPad minis
    {"iPad2,5",    0, 0},            // iPad mini (1st gen, Wi-Fi)
    {"iPad2,6",    3255536192, 4},   // iPad mini (1st gen, CDMA)
    {"iPad2,7",    3255536192, 4},   // iPad mini (1st gen, GSM)
    {"iPad4,4",    0, 0},            // iPad mini 2 (Wi-Fi)
    {"iPad4,5",    3554301762, 4},   // iPad mini 2 (Cellular)
    {"iPad4,6",    3554301762, 4},   // iPad mini 2 (Cellular, China)
    {"iPad4,7",    0, 0},            // iPad mini 3 (Wi-Fi)
    {"iPad4,8",    3554301762, 4},   // iPad mini 3 (Cellular)
    {"iPad4,9",    3554301762, 4},   // iPad mini 3 (Cellular, China)
    {"iPad5,1",    0, 0},            // iPad mini 4 (Wi-Fi)
    {"iPad5,2",    3840149528, 4},   // iPad mini 4 (Cellular)
    {"iPad11,1",   0, 0},            // iPad mini (5th gen, Wi-Fi)
    {"iPad11,2",   165673526, 12},   // iPad mini (5th gen, Cellular)
    {"iPad14,1",   0, 0},            // iPad mini (6th gen, Wi-Fi)
    {"iPad14,2",   495958265, 4},    // iPad mini (6th gen, Cellular)
    {"iPad16,1",   0, 0},            // iPad mini (A17 Pro, Wi-Fi)
    {"iPad16,2",   3452763205, 4},   // iPad mini (A17 Pro, Cellular)

    // iPad Airs
    {"iPad4,1",    0, 0},            // iPad Air (Wi-Fi)
    {"iPad4,2",    3554301762, 4},   // iPad Air (Cellular)
    {"iPad4,3",    3554301762, 4},   // iPad Air (Cellular, China)
    {"iPad5,3",    0, 0},            // iPad Air 2 (Wi-Fi)
    {"iPad5,4",    3840149528, 4},   // iPad Air 2 (Cellular)
    {"iPad11,3",   0, 0},            // iPad Air (3rd gen, Wi-Fi)
    {"iPad11,4",   165673526, 12},   // iPad Air (3rd gen, Cellular)
    {"iPad13,1",   0, 0},            // iPad Air (4th gen, Wi-Fi)
    {"iPad13,2",   524245983, 12},   // iPad Air (4th gen, Cellular)
    {"iPad13,16",  0, 0},            // iPad Air (5th gen, Wi-Fi)
    {"iPad13,17",  495958265, 4},    // iPad Air (5th gen, Cellular)
    {"iPad14,8",   0, 0},            // iPad Air (11-inch, M2, Wi-Fi)
    {"iPad14,9",   3452763205, 4},   // iPad Air (11-inch, M2, Cellular)
    {"iPad14,10",  0, 0},            // iPad Air (13-inch, M2, Wi-Fi)
    {"iPad14,11",  3452763205, 4},   // iPad Air (13-inch, M2, Cellular)
    {"iPad15,3",   0, 0},            // iPad Air (11-inch, M3, Wi-Fi)
    {"iPad15,4",   3452763205, 4},   // iPad Air (11-inch, M3, Cellular)
    {"iPad15,5",   0, 0},            // iPad Air (13-inch, M3, Wi-Fi)
    {"iPad15,6",   3452763205, 4},   // iPad Air (13-inch, M3, Cellular)

    // iPad Pros
    {"iPad6,3",    0, 0},            // iPad Pro (9.7-inch, Wi-Fi)
    {"iPad6,4",    3840149528, 4},   // iPad Pro (9.7-inch, Cellular)
    {"iPad6,7",    0, 0},            // iPad Pro (12.9-inch, 1st gen, Wi-Fi)
    {"iPad6,8",    3840149528, 4},   // iPad Pro (12.9-inch, 1st gen, Cellular)
    {"iPad7,1",    0, 0},            // iPad Pro (12.9-inch, 2nd gen, Wi-Fi)
    {"iPad7,2",    2315222105, 4},   // iPad Pro (12.9-inch, 2nd gen, Cellular)
    {"iPad7,3",    0, 0},            // iPad Pro (10.5-inch, Wi-Fi)
    {"iPad7,4",    2315222105, 4},   // iPad Pro (10.5-inch, Cellular)
    {"iPad8,1",    0, 0},            // iPad Pro (11-inch, 1st gen, Wi-Fi)
    {"iPad8,2",    0, 0},            // iPad Pro (11-inch, 1st gen, 1TB, Wi-Fi)
    {"iPad8,3",    165673526, 12},   // iPad Pro (11-inch, 1st gen, Cellular)
    {"iPad8,4",    165673526, 12},   // iPad Pro (11-inch, 1st gen, 1TB, Cellular)
    {"iPad8,5",    0, 0},            // iPad Pro (12.9-inch, 3rd gen, Wi-Fi)
    {"iPad8,6",    0, 0},            // iPad Pro (12.9-inch, 3rd gen, 1TB, Wi-Fi)
    {"iPad8,7",    165673526, 12},   // iPad Pro (12.9-inch, 3rd gen, Cellular)
    {"iPad8,8",    165673526, 12},   // iPad Pro (12.9-inch, 3rd gen, 1TB, Cellular)
    {"iPad8,9",    0, 0},            // iPad Pro (11-inch, 2nd gen, Wi-Fi)
    {"iPad8,10",   524245983, 12},   // iPad Pro (11-inch, 2nd gen, Cellular)
    {"iPad8,11",   0, 0},            // iPad Pro (12.9-inch, 4th gen, Wi-Fi)
    {"iPad8,12",   524245983, 12},   // iPad Pro (12.9-inch, 4th gen, Cellular)
    {"iPad13,4",   0, 0},            // iPad Pro (11-inch, 3rd gen, Wi-Fi)
    {"iPad13,5",   0, 0},            // iPad Pro (11-inch, 3rd gen, 2TB, Wi-Fi)
    {"iPad13,6",   3095201109, 4},   // iPad Pro (11-inch, 3rd gen, Cellular)
    {"iPad13,7",   3095201109, 4},   // iPad Pro (11-inch, 3rd gen, 2TB, Cellular)
    {"iPad13,8",   0, 0},            // iPad Pro (12.9-inch, 5th gen, Wi-Fi)
    {"iPad13,9",   0, 0},            // iPad Pro (12.9-inch, 5th gen, 2TB, Wi-Fi)
    {"iPad13,10",  3095201109, 4},   // iPad Pro (12.9-inch, 5th gen, Cellular)
    {"iPad13,11",  3095201109, 4},   // iPad Pro (12.9-inch, 5th gen, 2TB, Cellular)
    {"iPad14,3",   0, 0},            // iPad Pro (11-inch, 4th gen, Wi-Fi)
    {"iPad14,4",   3559316616, 4},   // iPad Pro (11-inch, 4th gen, Cellular)
    {"iPad14,5",   0, 0},            // iPad Pro (12.9-inch, 6th gen, Wi-Fi)
    {"iPad14,6",   3559316616, 4},   // iPad Pro (12.9-inch, 6th gen, Cellular)
    {"iPad16,3",   0, 0},            // iPad Pro (11-inch, M4, Wi-Fi)
    {"iPad16,4",   3452763205, 4},   // iPad Pro (11-inch, M4, Cellular)
    {"iPad16,5",   0, 0},            // iPad Pro (13-inch, M4, Wi-Fi)
    {"iPad16,6",   3452763205, 4},   // iPad Pro (13-inch, M4, Cellular)

    // Apple Watches
    {"Watch1,1",   0, 0},            // Apple Watch 1st gen (38mm)
    {"Watch1,2",   0, 0},            // Apple Watch 1st gen (42mm)
    {"Watch2,6",   0, 0},            // Apple Watch Series 1 (38mm)
    {"Watch2,7",   0, 0},            // Apple Watch Series 1 (42mm)
    {"Watch2,3",   0, 0},            // Apple Watch Series 2 (38mm)
    {"Watch2,4",   0, 0},            // Apple Watch Series 2 (42mm)
    {"Watch3,1",   3840149528, 4},   // Apple Watch Series 3 (38mm, GPS + Cellular)
    {"Watch3,2",   3840149528, 4},   // Apple Watch Series 3 (42mm, GPS + Cellular)
    {"Watch3,3",   0, 0},            // Apple Watch Series 3 (38mm, GPS)
    {"Watch3,4",   0, 0},            // Apple Watch Series 3 (42mm, GPS)
    {"Watch4,1",   0, 0},            // Apple Watch Series 4 (40mm, GPS)
    {"Watch4,2",   0, 0},            // Apple Watch Series 4 (44mm, GPS)
    {"Watch4,3",   744114402, 12},   // Apple Watch Series 4 (40mm, GPS + Cellular)
    {"Watch4,4",   744114402, 12},   // Apple Watch Series 4 (44mm, GPS + Cellular)
    {"Watch5,1",   0, 0},            // Apple Watch Series 5 (40mm, GPS)
    {"Watch5,2",   0, 0},            // Apple Watch Series 5 (44mm, GPS)
    {"Watch5,3",   744114402, 12},   // Apple Watch Series 5 (40mm, GPS + Cellular)
    {"Watch5,4",   744114402, 12},   // Apple Watch Series 5 (44mm, GPS + Cellular)
    {"Watch5,9",   0, 0},            // Apple Watch SE (40mm, GPS)
    {"Watch5,10",  0, 0},            // Apple Watch SE (44mm, GPS)
    {"Watch5,11",  744114402, 12},   // Apple Watch SE (40mm, GPS + Cellular)
    {"Watch5,12",  744114402, 12},   // Apple Watch SE (44mm, GPS + Cellular)
    {"Watch6,1",   0, 0},            // Apple Watch Series 6 (40mm, GPS)
    {"Watch6,2",   0, 0},            // Apple Watch Series 6 (44mm, GPS)
    {"Watch6,3",   744114402, 12},   // Apple Watch Series 6 (40mm, GPS + Cellular)
    {"Watch6,4",   744114402, 12},   // Apple Watch Series 6 (44mm, GPS + Cellular)
    {"Watch6,6",   0, 0},            // Apple Watch Series 7 (41mm, GPS)
    {"Watch6,7",   0, 0},            // Apple Watch Series 7 (45mm, GPS)
    {"Watch6,8",   744114402, 12},   // Apple Watch Series 7 (41mm, GPS + Cellular)
    {"Watch6,9",   744114402, 12},   // Apple Watch Series 7 (45mm, GPS + Cellular)
    {"Watch6,10",  0, 0},            // Apple Watch SE (2nd gen, 40mm, GPS)
    {"Watch6,11",  0, 0},            // Apple Watch SE (2nd gen, 44mm, GPS)
    {"Watch6,12",  744114402, 12},   // Apple Watch SE (2nd gen, 40mm, GPS + Cellular)
    {"Watch6,13",  744114402, 12},   // Apple Watch SE (2nd gen, 44mm, GPS + Cellular)
    {"Watch6,14",  0, 0},            // Apple Watch Series 8 (41mm, GPS)
    {"Watch6,15",  0, 0},            // Apple Watch Series 8 (45mm, GPS)
    {"Watch6,16",  744114402, 12},   // Apple Watch Series 8 (41mm, GPS + Cellular)
    {"Watch6,17",  744114402, 12},   // Apple Watch Series 8 (45mm, GPS + Cellular)
    {"Watch6,18",  744114402, 12},   // Apple Watch Ultra (49mm, GPS + Cellular)
    {"Watch7,1",   0, 0},            // Apple Watch Series 9 (41mm, GPS)
    {"Watch7,2",   0, 0},            // Apple Watch Series 9 (45mm, GPS)
    {"Watch7,3",   744114402, 12},   // Apple Watch Series 9 (41mm, GPS + Cellular)
    {"Watch7,4",   744114402, 12},   // Apple Watch Series 9 (45mm, GPS + Cellular)
    {"Watch7,5",   744114402, 12},   // Apple Watch Ultra 2 (49mm, GPS + Cellular)
    {"Watch7,8",   0, 0},            // Apple Watch Series 10 (42mm, GPS)
    {"Watch7,9",   0, 0},            // Apple Watch Series 10 (46mm, GPS)
    {"Watch7,10",  744114402, 12},   // Apple Watch Series 10 (42mm, GPS + Cellular)
    {"Watch7,11",  744114402, 12},   // Apple Watch Series 10 (46mm, GPS + Cellular)
    {"Watch7,13",  0, 0},            // Apple Watch SE (3rd gen, 40mm, GPS)
    {"Watch7,14",  0, 0},            // Apple Watch SE (3rd gen, 44mm, GPS)
    {"Watch7,15",  0, 0},            // Apple Watch SE (3rd gen, 40mm, GPS + Cellular)
    {"Watch7,16",  0, 0},            // Apple Watch SE (3rd gen, 44mm, GPS + Cellular)
    {"Watch7,12",  0, 0},            // Apple Watch Ultra 3 (49mm, GPS + Cellular)
    {"Watch7,17",  0, 0},            // Apple Watch Series 11 (42mm, GPS)
    {"Watch7,18",  0, 0},            // Apple Watch Series 11 (46mm, GPS)
    {"Watch7,19",  0, 0},            // Apple Watch Series 11 (42mm, GPS + Cellular)
    {"Watch7,20",  0, 0},            // Apple Watch Series 11 (46mm, GPS + Cellular)

    // HomePods
    {"AudioAccessory1,1",   0, 0},   // HomePod 1st gen
    {"AudioAccessory1,2",   0, 0},   // HomePod 1st gen (2018)
    {"AudioAccessory5,1",   0, 0},   // HomePod mini
    {"AudioAccessory6,1",   0, 0},   // HomePod 2nd gen

    // Apple TVs
    {"AppleTV1,1",   0, 0},  // 1st gen
    {"AppleTV2,1",   0, 0},  // 2nd gen
    {"AppleTV3,1",   0, 0},  // 3rd gen
    {"AppleTV3,2",   0, 0},  // 3rd gen (2013)
    {"AppleTV5,3",   0, 0},  // 4th gen
    {"AppleTV6,2",   0, 0},  // 4K 1st gen
    {"AppleTV11,1",  0, 0},  // 4K 2nd gen
    {"AppleTV14,1",  0, 0},  // 4K 3rd gen
    {NULL, 0, 0}
};


#pragma mark static helpers
static void printline(int percent){
    printf("%03d [",percent);for (int i=0; i<100; i++) putchar((percent >0) ? ((--percent > 0) ? '=' : '>') : ' ');
    printf("]");
}
static void fragmentzip_callback(unsigned int progress){
    printf("\x1b[A\033[J"); //clear 2 lines
    printline((int)progress);
    printf("\n");
}

#pragma mark tsschecker functions
#pragma mark parsers
uint64_t tsschecker::parseECID(const char *ecid){
    bool isHex = false;
    uint64_t ret = 0;
    
    if (strncasecmp(ecid, "0x", 2) == 0) {
        isHex = true;
        ecid += 2;
    }
    retassure(*ecid, "Empty number");

    if (!isHex){
        //try parse decimal, fall back to hex if there are non-decimal chars
        isHex = ecid[strspn(ecid, "0123456789")] != '\0';
    }

    if (!isHex){
        for (; *ecid; ecid++) {
            uint64_t digit = *ecid - '0';
            retassure(ret <= (UINT64_MAX - digit) / 10, "Number too large");
            ret = ret * 10 + digit;
        }
        return ret;
    }

    retassure(strlen(ecid) <= 16, "Hex number too large");
    for (; *ecid; ecid++) {
        char c = *ecid;
        ret <<= 4;
        if (c >= '0' && c<='9') {
            ret += c - '0';
        }else if (c >= 'a' && c <= 'f'){
            ret += 10 + c - 'a';
        }else if (c >= 'A' && c <= 'F'){
            ret += 10 + c - 'A';
        }else{
            reterror("Got unexpected char '%c'",c);
        }
    }
    return ret;
}

tihmstar::Mem tsschecker::parseHex(const char *hexstr){
    if (strncasecmp(hexstr, "0x", 2) == 0) hexstr += 2;
    tihmstar::Mem ret;
    ret.resize(strlen(hexstr));
    size_t i = 0;
    while (*hexstr) {
        char c = *hexstr++;
        uint8_t &v = ret.data()[(i++)/2];
        v <<= 4;
        if (c >= '0' && c<='9') {
            v += c - '0';
        }else if (c >= 'a' && c <= 'f'){
            v += 10 + c - 'a';
        }else if (c >= 'A' && c <= 'F'){
            v += 10 + c - 'A';
        }else{
            reterror("Got unexpected char '%c'",c);
        }
    }
    retassure((i&1) == 0, "Odd number of char in hexstr");
    ret.resize(i/2);
    return ret;
}

tsschecker::firmwareVersion tsschecker::firmwareVersionFromBuildManifest(plist_t pBuildManifest){
    tsschecker::firmwareVersion ret;
    {
        plist_t pVal = NULL;
        retassure(pVal = plist_dict_get_item(pBuildManifest, "ProductVersion"), "ProductVersion not set");
        retassure(plist_get_node_type(pVal) == PLIST_STRING, "ProductVersion not STRING");
        {
            const char *s = NULL;
            uint64_t slen = 0;
            retassure(s = plist_get_string_ptr(pVal, &slen), "Failed to get ProductVersion");
            ret.version = {s,s+slen};
        }
    }
    
    {
        plist_t pVal = NULL;
        retassure(pVal = plist_dict_get_item(pBuildManifest, "ProductBuildVersion"), "ProductBuildVersion not set");
        retassure(plist_get_node_type(pVal) == PLIST_STRING, "ProductBuildVersion not STRING");
        {
            const char *s = NULL;
            uint64_t slen = 0;
            retassure(s = plist_get_string_ptr(pVal, &slen), "Failed to get ProductBuildVersion");
            ret.build = {s,s+slen};
        }
    }
    
    return ret;
}


#pragma mark downloaders
tihmstar::Mem tsschecker::downloadFile(const char *url){
    debug("Downloading file '%s'",url);
    CURL *mcurl = NULL;
    cleanup([&]{
        safeFreeCustom(mcurl, curl_easy_cleanup);
    });
    tihmstar::Mem ret;
    CURLcode res = {};
    
    mcurl = curl_easy_init();
    
    curl_easy_setopt(mcurl, CURLOPT_URL, url);
    curl_easy_setopt(mcurl, CURLOPT_TIMEOUT, 20L); //20 sec
    curl_easy_setopt(mcurl, CURLOPT_WRITEFUNCTION, (size_t (*)(void *, size_t, size_t, void *))[](void *contents, size_t size, size_t nmemb, void *userp)->size_t{
        uint8_t *ptr = (uint8_t*)contents;
        size_t realsize = size * nmemb;
        tihmstar::Mem *mem = (tihmstar::Mem *)userp;;
        mem->append(ptr, realsize);
        return realsize;
    });
    curl_easy_setopt(mcurl, CURLOPT_WRITEDATA, (void *)&ret);
    
    retassure((res = curl_easy_perform(mcurl)) == CURLE_OK, "curl failed with error=%d",res);
    return ret;
}

tihmstar::Mem tsschecker::downloadFileFromIPSW(const char *ipswurl, const char *path){
    fragmentzip_t *fz = NULL;
    char *buf = NULL;
    cleanup([&]{
        safeFree(buf);
        safeFreeCustom(fz, fragmentzip_close);
    });
    size_t bufSize = 0;
    int err = 0;
    
    retassure(fz = fragmentzip_open(ipswurl), "Failed to open url: '%s'",ipswurl);
    retassure(!(err = fragmentzip_download_to_memory(fz, path, &buf, &bufSize, fragmentzip_callback)), "Failed to download '%s' with err=%d",path,err);
    {
        Mem ret{buf,bufSize}; buf = NULL; bufSize = 0;
        return ret;
    }
}

plist_t tsschecker::getBuildManifestFromUrl(const char *ipswurl){
    fragmentzip_t *fz = NULL;
    char *buf = NULL;
    cleanup([&]{
        safeFree(buf);
        safeFreeCustom(fz, fragmentzip_close);
    });
    size_t bufSize = 0;
    int err = 0;
    
    retassure(fz = fragmentzip_open(ipswurl), "Failed to open url: '%s'",ipswurl);
    
    if ((err = fragmentzip_download_to_memory(fz, "BuildManifest.plist", &buf, &bufSize, fragmentzip_callback))){
        debug("Failed to get BuildManifest.plist, retrying with AssetData/boot/BuildManifest.plist");
        err = fragmentzip_download_to_memory(fz, "AssetData/boot/BuildManifest.plist", &buf, &bufSize, fragmentzip_callback);
    }
    retassure(!err, "Failed to download BuildManifest.plist");

    {
        plist_t p_ret = NULL;
        retassure(plist_from_memory(buf, (uint32_t)bufSize, &p_ret, NULL) == PLIST_ERR_SUCCESS && p_ret, "Failed to parse BuildManifest.plist");
        return p_ret;
    }
}

tsschecker::firmwareVersion tsschecker::getLatestFirmwareForDevice(uint32_t cpid, uint32_t bdid, bool ota){
    static int usecache = 0;
    FirmwareAPI_IPSWME fapi(ota);
    cleanup([&]{
        try {
            if (usecache == 1) fapi.storecache();
        } catch (...) {
            //
        }
    });
    {
        //load firmwares
        bool isLoaded = false;
        try {
            if (usecache++ > 0){
                fapi.loadcache();
                isLoaded = true;
            }
        } catch (tihmstar::exception &e) {
            usecache = 1; //store new cache
            debug("Failed to load cache with error:\n%s",e.dumpStr().c_str());
        }
        if (!isLoaded) fapi.load();
    }
    const char *boardType = getBoardTypeFromCPIDandBDID(cpid, bdid);
    retcustomassure(TSSException_DBLookupFailed, boardType, "Unknown device CPID:0x%x BDID:0x%x",cpid,bdid);
    return fapi.getURLForBoardAndBuild(boardType);
}

#pragma mark file handling
plist_t tsschecker::readPlist(const char *path){
    auto f = tihmstar::readFile(path);
    {
        plist_t ret = NULL;
        retassure(plist_from_memory((char*)f.data(), (uint32_t)f.size(), &ret, NULL) == PLIST_ERR_SUCCESS && ret, "Failed to parse plist '%s'",path);
        return ret;
    }
}

void tsschecker::writePlist(const char *path, plist_t plist){
    char *xml = NULL;
    cleanup([&]{
        safeFree(xml);
    });
    uint32_t xmlSize = 0;
    plist_to_xml(plist, &xml, &xmlSize);
    writeFile(path, xml, xmlSize);
}

std::string tsschecker::getTicketSavePathFromRequest(const char *path, const TssRequest &req){
    struct stat st = {};
    if (stat(path, &st) || !S_ISDIR(st.st_mode)) return path;
    //dst path is a directory, generate a filename
    std::string ret = path;
    if (ret.back() != '/') ret += '/';
    
    ret += req.getProductType() + "_";
    {
        char buf[100] = {};
        snprintf(buf, sizeof(buf), "0x%llx",req.getECID());
        ret += buf;
        ret += "_";
    }
    ret += req.getProductVersion() + "-";
    ret += req.getBuildVersion() + "_";
    try {
        ret += req.getAPNonceString();
    } catch (...) {
        //
    }
    ret += ".shsh";
    
    try {
        if (req.getGenerator()) ret += "2";
    } catch (...) {
        //
    }
    return ret;
}

#pragma mark printing
void tsschecker::printListOfDevices(std::vector<std::string> devicesList){
    std::string lastdevtype;
    int lastMajor = 0;
    for (auto dev : devicesList) {
        ssize_t digitpos = 0;
        for (auto c : dev){
            if (!isdigit(c)) digitpos++;
            else break;
        }
        std::string devtype = dev.substr(0,digitpos);
        int major = atoi(dev.substr(digitpos).c_str());
        if (lastdevtype != devtype) {
            lastdevtype = devtype;
            lastMajor = major;
            printf("\n\n[%s]\n",lastdevtype.c_str());
        }
        if (lastMajor != major){
            lastMajor = major;
            printf("\n");
        }
        printf("%s ",dev.c_str());
    }
}

void tsschecker::printListOfVersions(std::vector<std::string> versionList){
    int lastMajor = 0;
    std::string lastVersion;
    for (auto vers : versionList) {
        int major = atoi(vers.c_str());
        if (lastMajor != major) {
            lastMajor = major;
            printf("\n[%2d] ",major);
        }
        if (lastVersion != vers){
            lastVersion = vers;
            printf("%10s ",vers.c_str());
        }
    }
    printf("\n\n");
}

#pragma mark device database
const char *tsschecker::getBoardTypeFromProductType(const char *productType){
    for (irecv_device_t dev = irecv_devices_get_all(); dev->hardware_model; dev++) {
        if (strcasecmp(dev->product_type, productType) == 0){
            switch (dev->chip_id) {
                case 0x8000:
                case 0x8003:
                    /*
                        Special case for CPID 0x8000 and 0x8003.
                        Apple produced the exact same device twice but with these different chips.
                        Thus, we can't uniquely identify one of these CPIDs solely by looking at the device type :(
                     */
                    return NULL;
                    
                default:
                    return dev->hardware_model;
            }
        }
    }
    return NULL;
}

const char *tsschecker::getBoardTypeFromCPIDandBDID(uint32_t cpid, uint32_t bdid){
    for (irecv_device_t dev = irecv_devices_get_all(); dev->hardware_model; dev++) {
        if (dev->chip_id == cpid && dev->board_id == bdid) return dev->hardware_model;
    }
    return NULL;
}

const char *tsschecker::getProductTypeFromBoardType(const char *boardType){
    for (irecv_device_t dev = irecv_devices_get_all(); dev->hardware_model; dev++) {
        if (strcasecmp(dev->hardware_model, boardType) == 0) return dev->product_type;
    }
    return NULL;
}

const char *tsschecker::getProductTypeFromCPIDandBDID(uint32_t cpid, uint32_t bdid){
    for (irecv_device_t dev = irecv_devices_get_all(); dev->hardware_model; dev++) {
        if (dev->chip_id == cpid && dev->board_id == bdid) return dev->product_type;
    }
    return NULL;
}

uint32_t tsschecker::getCPIDForBoardType(const char *boardType){
    for (irecv_device_t dev = irecv_devices_get_all(); dev->hardware_model; dev++) {
        if (strcasecmp(dev->hardware_model, boardType) == 0) return dev->chip_id;
    }
    retcustomerror(TSSException_DBLookupFailed, "Failed to get CPID");
}

uint32_t tsschecker::getBDIDForBoardType(const char *boardType){
    for (irecv_device_t dev = irecv_devices_get_all(); dev->hardware_model; dev++) {
        if (strcasecmp(dev->hardware_model, boardType) == 0) return dev->board_id;
    }
    retcustomerror(TSSException_DBLookupFailed, "Failed to get BDID");
}

uint32_t tsschecker::getCPIDForProductType(const char *productType){
    for (irecv_device_t dev = irecv_devices_get_all(); dev->hardware_model; dev++) {
        if (strcasecmp(dev->product_type, productType) == 0){
            switch (dev->chip_id) {
                case 0x8000:
                case 0x8003:
                    /*
                        Special case for CPID 0x8000 and 0x8003.
                        Apple produced the exact same device twice but with these different chips.
                        Thus, we can't uniquely identify one of these CPIDs solely by looking at the device type :(
                     */
                    retcustomerror(TSSException_DBLookupFailed, "Failed to get CPID");

                default:
                    return dev->chip_id;
            }
        }
    }
    retcustomerror(TSSException_DBLookupFailed, "Failed to get CPID");
}

uint32_t tsschecker::getBDIDForProductType(const char *productType){
    for (irecv_device_t dev = irecv_devices_get_all(); dev->hardware_model; dev++) {
        if (strcasecmp(dev->product_type, productType) == 0) return dev->board_id;
    }
    retcustomerror(TSSException_DBLookupFailed, "Failed to get CPID");
}

tsschecker::nonceType tsschecker::nonceTypeForCPID(uint32_t cpid){
    switch (cpid) {
        case 0x8900:
        case 0x8720:
            return kNonceTypeNone;

        case 0x8920:
        case 0x8922:
        case 0x8930:
        case 0x8940:
        case 0x8942:
        case 0x8945:
        case 0x8947:
        case 0x8950:
        case 0x8955:
        case 0x8960:
        case 0x7000:
        case 0x7001:
        case 0x7002:
        case 0x8000:
        case 0x8003:
            return kNonceTypeSHA1;

        case 0x8010:
        case 0x8011:
        case 0x8015:
        case 0x8020:
        case 0x8027:
        case 0x8030:
        case 0x8101:
        case 0x8110:
        case 0x8120:
            return kNonceTypeSHA384;

        default:
            //every device newer than the ones above uses SHA384
            for (irecv_device_t dev = irecv_devices_get_all(); dev->hardware_model; dev++) {
                if (dev->chip_id == cpid) return kNonceTypeSHA384;
            }
            warning("UNKNOWN CPID 0x%x! Assuming SHA384",cpid);
            return kNonceTypeSHA384;
    }
}

int64_t tsschecker::getGoldCertIDForDevice(const char *productType){
    retassure(productType, "No ProductType given for baseband database lookup");
    bbdevice *bbdevs = bbdevices;
    while (bbdevs->deviceModel && strcasecmp(bbdevs->deviceModel, productType) != 0) bbdevs++;
    retassure(bbdevs->deviceModel, "Failed to find device '%s' in baseband database",productType);
    return bbdevs->bbgcid;
}

size_t tsschecker::getSNUMLenForDevice(const char *productType){
    retassure(productType, "No ProductType given for baseband database lookup");
    bbdevice *bbdevs = bbdevices;
    while (bbdevs->deviceModel && strcasecmp(bbdevs->deviceModel, productType) != 0) bbdevs++;
    retassure(bbdevs->deviceModel, "Failed to find device '%s' in baseband database",productType);
    return bbdevs->bbsnumSize;
}


#pragma mark plist manipulation functions
void *tsschecker::iterateOverPlistElementsInArray(plist_t array, std::function<void *(plist_t)> cb){
    plist_array_iter p_iter = NULL;
    cleanup([&]{
        safeFree(p_iter);
    });
    plist_t pElem = NULL;
    if (plist_get_node_type(array) != PLIST_ARRAY) return NULL;
    plist_array_new_iter(array, &p_iter);
    
    for (plist_array_next_item(array, p_iter, &pElem); pElem; plist_array_next_item(array, p_iter, &pElem)) {
        if (void *r = cb(pElem)) return r;
    }
    return NULL;
}

void *tsschecker::iterateOverPlistElementsInDict(plist_t dict, std::function<void *(const char *, plist_t)> cb){
    plist_dict_iter p_iter = NULL;
    char *key = NULL;
    cleanup([&]{
        safeFree(key);
        safeFree(p_iter);
    });
    plist_t pElem = NULL;
    if (plist_get_node_type(dict) != PLIST_DICT) return NULL;
    plist_dict_new_iter(dict, &p_iter);
        
    for (plist_dict_next_item(dict, p_iter, &key, &pElem); pElem; safeFree(key),plist_dict_next_item(dict, p_iter, &key, &pElem)) {
        if (void *r = cb(key, pElem)) return r;
    }
    return NULL;
}

void tsschecker::dumpplist(plist_t p){
    char *xml = NULL;
    cleanup([&]{
        safeFree(xml);
    });
    uint32_t xmlSize = 0;
    plist_to_xml(p, &xml, &xmlSize);
    printf("%.*s\n",xmlSize,xml);
}

plist_t tsschecker::buildIdentityFromRestorePlist(plist_t restorePlist){
    plist_t p_buildIdentity = NULL;
    plist_t p_Manifest = NULL;
    plist_t p_Info = NULL;
    char *boardConfig_val = NULL;
    char *kernelpath = NULL;
    char *updateRamdiskPath = NULL;
    char *userRamdiskPath = NULL;
    cleanup([&]{
        safeFree(userRamdiskPath);
        safeFree(updateRamdiskPath);
        safeFree(kernelpath);
        safeFree(boardConfig_val);
        safeFreeCustom(p_Info, plist_free);
        safeFreeCustom(p_Manifest, plist_free);
        safeFreeCustom(p_buildIdentity, plist_free);
    });
    plist_t p_DeviceMap = NULL;
    plist_t p_DeviceMap_0 = NULL;
    plist_t p_CPID = NULL;
    plist_t p_BDID = NULL;
    plist_t p_BoardConfig = NULL;
    plist_t p_RestoreKernelCaches = NULL;
    plist_t p_ReleaseKernelCache = NULL;
    plist_t p_RestoreRamDisks = NULL;
    plist_t p_UpdateRamdisk = NULL;
    plist_t p_UserRamdisk = NULL;
    plist_t p_ProductType = NULL;
    uint64_t cpid_val = 0;

    retassure(p_buildIdentity = plist_new_dict(), "Failed to alloc new buildidentity");
    retassure(p_Manifest = plist_new_dict(), "Failed to alloc new manifest");
    retassure(p_Info = plist_new_dict(), "Failed to alloc new p_Info");

    retassure(p_DeviceMap = plist_dict_get_item(restorePlist, "DeviceMap"), "Failed to get 'DeviceMap'");
    retassure(p_ProductType = plist_dict_get_item(restorePlist, "ProductType"), "Failed to get 'ProductType'");
    retassure(p_DeviceMap_0 = plist_array_get_item(p_DeviceMap, 0), "Failed to get DeviceMap[0]");
    retassure(p_CPID = plist_dict_get_item(p_DeviceMap_0, "CPID"), "Failed to get 'CPID'");
    retassure(p_BDID = plist_dict_get_item(p_DeviceMap_0, "BDID"), "Failed to get 'BDID'");
    retassure(p_BoardConfig = plist_dict_get_item(p_DeviceMap_0, "BoardConfig"), "Failed to get 'BoardConfig'");
    if (!(p_RestoreKernelCaches = plist_dict_get_item(restorePlist, "RestoreKernelCaches"))){
        error("Failed to get 'RestoreKernelCaches'");
    }else{
        retassure(p_ReleaseKernelCache = plist_dict_get_item(p_RestoreKernelCaches, "Release"), "Failed to get 'RestoreKernelCaches'->'Release'");
    }
    if (!(p_RestoreRamDisks = plist_dict_get_item(restorePlist, "RestoreRamDisks"))){
        error("Failed to get 'RestoreRamDisks''");
    }else{
        retassure(p_UpdateRamdisk = plist_dict_get_item(p_RestoreRamDisks, "Update"), "Failed to get 'RestoreRamDisks'->'Update'");
        retassure(p_UserRamdisk = plist_dict_get_item(p_RestoreRamDisks, "User"), "Failed to get 'RestoreRamDisks'->'User'");
    }

    if (p_CPID){
        retassure(plist_get_node_type(p_CPID) == PLIST_INT, "Failed to get cpid val");
        plist_get_uint_val(p_CPID, &cpid_val);
        char buf[0x100] = {};
        snprintf(buf, sizeof(buf), "0x%llx",cpid_val);
        plist_dict_set_item(p_buildIdentity, "ApChipID", plist_new_string(buf));
    }

    if (p_BDID){
        uint64_t val = 0;
        retassure(plist_get_node_type(p_BDID) == PLIST_INT, "Failed to get bdid val");
        plist_get_uint_val(p_BDID, &val);
        char buf[0x100] = {};
        snprintf(buf, sizeof(buf), "0x%llx",val);
        plist_dict_set_item(p_buildIdentity, "ApBoardID", plist_new_string(buf));
    }

    if (p_BoardConfig){
        plist_get_string_val(p_BoardConfig, &boardConfig_val);
        retassure(boardConfig_val, "Failed to get board config val");
    }
    
    if (p_ReleaseKernelCache) {
        plist_get_string_val(p_ReleaseKernelCache, &kernelpath);
        retassure(kernelpath, "Failed to get kernelpath");
    }

    if (p_UpdateRamdisk) {
        plist_get_string_val(p_UpdateRamdisk, &updateRamdiskPath);
        retassure(updateRamdiskPath, "Failed to get updateRamdiskPath");
    }

    if (p_UserRamdisk) {
        plist_get_string_val(p_UserRamdisk, &userRamdiskPath);
        retassure(userRamdiskPath, "Failed to get userRamdiskPath");
    }

    auto addManifestElement = [](plist_t p_manifest, const char *element, const char *path)->void{
        plist_t p_Info = NULL;
        plist_t p_elem = NULL;
        cleanup([&]{
            safeFreeCustom(p_elem, plist_free);
            safeFreeCustom(p_Info, plist_free);
        });
        p_elem = plist_new_dict();
        p_Info = plist_new_dict();
        plist_dict_set_item(p_Info, "Path", plist_new_string(path));
        plist_dict_set_item(p_elem, "Info", p_Info); p_Info = NULL;
        plist_dict_set_item(p_manifest, element, p_elem); p_elem = NULL;
    };

    
    {
        char buf[0x100] = {};
        snprintf(buf, sizeof(buf), "Firmware/dfu/iBSS.%s.RELEASE.dfu",boardConfig_val);
        addManifestElement(p_Manifest, "iBSS", buf);

        snprintf(buf, sizeof(buf), "Firmware/dfu/iBEC.%s.RELEASE.dfu",boardConfig_val);
        addManifestElement(p_Manifest, "iBEC", buf);

        snprintf(buf, sizeof(buf), "Firmware/all_flash/all_flash.%s.production/DeviceTree.%s.img3",boardConfig_val,boardConfig_val);
        addManifestElement(p_Manifest, "DeviceTree", buf);

        snprintf(buf, sizeof(buf), "Firmware/all_flash/all_flash.%s.production/LLB.%s.RELEASE.img3",boardConfig_val,boardConfig_val);
        addManifestElement(p_Manifest, "LLB", buf);

        snprintf(buf, sizeof(buf), "Firmware/all_flash/all_flash.%s.production/iBoot.%s.RELEASE.img3",boardConfig_val,boardConfig_val);
        addManifestElement(p_Manifest, "iBoot", buf);
        
        snprintf(buf, sizeof(buf), "Firmware/all_flash/all_flash.%s.production/needservice.s5l%xx.img3",boardConfig_val,(uint32_t)cpid_val);
        addManifestElement(p_Manifest, "NeedService", buf);

        snprintf(buf, sizeof(buf), "Firmware/all_flash/all_flash.%s.production/batterylow0.s5l%xx.img3",boardConfig_val,(uint32_t)cpid_val);
        addManifestElement(p_Manifest, "BatteryLow0", buf);

        snprintf(buf, sizeof(buf), "Firmware/all_flash/all_flash.%s.production/recoverymode.s5l%xx.img3",boardConfig_val,(uint32_t)cpid_val);
        addManifestElement(p_Manifest, "RecoveryMode", buf);

        snprintf(buf, sizeof(buf), "Firmware/all_flash/all_flash.%s.production/glyphcharging.s5l%xx.img3",boardConfig_val,(uint32_t)cpid_val);
        addManifestElement(p_Manifest, "BatteryCharging", buf);

        snprintf(buf, sizeof(buf), "Firmware/all_flash/all_flash.%s.production/applelogo.s5l%xx.img3",boardConfig_val,(uint32_t)cpid_val);
        addManifestElement(p_Manifest, "AppleLogo", buf);

        snprintf(buf, sizeof(buf), "Firmware/all_flash/all_flash.%s.production/batterylow1.s5l%xx.img3",boardConfig_val,(uint32_t)cpid_val);
        addManifestElement(p_Manifest, "BatteryLow1", buf);

        snprintf(buf, sizeof(buf), "Firmware/all_flash/all_flash.%s.production/glyphplugin.s5l%xx.img3",boardConfig_val,(uint32_t)cpid_val);
        addManifestElement(p_Manifest, "BatteryPlugin", buf);

        if (kernelpath) {
            addManifestElement(p_Manifest, "KernelCache", kernelpath);
            addManifestElement(p_Manifest, "RestoreKernelCache", kernelpath);
        }

        if (userRamdiskPath) {
            //erase install
            addManifestElement(p_Manifest, "RestoreRamDisk", userRamdiskPath);
        }
    }

    {
        //erase mode
        plist_dict_set_item(p_Info, "RestoreBehavior", plist_new_string("Erase"));
        plist_dict_set_item(p_Info, "Variant", plist_new_string("Customer Erase Install (IPSW)"));
        plist_dict_set_item(p_Info, "DeviceClass", plist_copy(p_BoardConfig));
        plist_dict_set_item(p_buildIdentity, "Info", plist_copy(p_Info));
    }
    
    {
        plist_t p_BuildIdentities = NULL;
        cleanup([&]{
            safeFreeCustom(p_BuildIdentities, plist_free);
        });
        assure(p_BuildIdentities = plist_new_array());
        
        plist_dict_set_item(p_buildIdentity, "Manifest", plist_copy(p_Manifest));
        plist_array_append_item(p_BuildIdentities, plist_copy(p_buildIdentity));
        
        {
            //add update mode
            plist_dict_set_item(p_Info, "RestoreBehavior", plist_new_string("Update"));
            plist_dict_set_item(p_Info, "Variant", plist_new_string("Customer Upgrade Install (IPSW)"));
            plist_dict_set_item(p_buildIdentity, "Info", plist_copy(p_Info));

            if (updateRamdiskPath) {
                addManifestElement(p_Manifest, "RestoreRamDisk", updateRamdiskPath);
            }

            plist_dict_set_item(p_buildIdentity, "Manifest", plist_copy(p_Manifest));
            plist_array_append_item(p_BuildIdentities, plist_copy(p_buildIdentity));
        }
        
        plist_dict_set_item(restorePlist, "BuildIdentities", p_BuildIdentities);p_BuildIdentities = NULL;
    }
    
    {
        plist_t p_SupportedProductTypes = NULL;
        cleanup([&]{
            safeFreeCustom(p_SupportedProductTypes, plist_free);
        });
        p_SupportedProductTypes = plist_new_array();
        plist_array_append_item(p_SupportedProductTypes, plist_copy(p_ProductType));
        plist_dict_set_item(restorePlist, "SupportedProductTypes", p_SupportedProductTypes); p_SupportedProductTypes = NULL;
    }
    
    {
        plist_t p_bi = NULL;
        assure(p_bi = plist_dict_get_item(restorePlist, "BuildIdentities"));
        return plist_array_get_item(p_bi, 0);
    }
}
