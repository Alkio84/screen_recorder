//
//  ScreenSize.h
//  ScreenRecorder
//
//  Created by Checco on 08/11/2021.
//  Copyright © 2021 Checco. All rights reserved.
//

#ifndef ScreenSize_h
#define ScreenSize_h

#if WIN32
#include <windows.h>
#else
#include <CoreGraphics/CoreGraphics.h>
#endif

#endif

class ScreenSize {
public:
    static void getScreenResolution(unsigned int& width, unsigned int& height) {
#if WIN32
        width = (int)GetSystemMetrics(SM_CXVIRTUALSCREEN);
        height = (int)GetSystemMetrics(SM_CYVIRTUALSCREEN);

        /*std::cout << "Numero Schermi: " << GetSystemMetrics(SM_CMONITORS) << std::endl;
        std::cout << "Larghezza Schermo: " << GetSystemMetrics(SM_CXVIRTUALSCREEN) << std::endl;
        std::cout << "Altezza Schermo: " << GetSystemMetrics(SM_CYVIRTUALSCREEN) << std::endl;
        std::cout << "Lato sinistro: " << GetSystemMetrics(SM_XVIRTUALSCREEN) << std::endl;
        std::cout << "Lato alto: " << GetSystemMetrics(SM_YVIRTUALSCREEN) << std::endl;*/
#else
        auto mainDisplayId = CGMainDisplayID();
        
        /*
        std::cout << "Current resolution was "
        << CGDisplayPixelsWide(mainDisplayId) << 'x'
        << CGDisplayPixelsHigh(mainDisplayId) << std::endl
        << "Supported resolution modes:";
        */
        
        /* In MacOS sono supportate diverse risoluzioni */
        /* Nell'array la mia risoluzione utilizzata è l'ultima (2560 x 1600) */
        /* Sarebbe bello avere un metodo per stampare le diverse risoluzioni ma c'è da implementarlo anche per windows */
        
        auto modes = CGDisplayCopyAllDisplayModes(mainDisplayId, nullptr);
        auto count = CFArrayGetCount(modes);
        CGDisplayModeRef mode;
        for(auto c=count;c--;){
            mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, c);
            width = CGDisplayModeGetWidth(mode);
            height = CGDisplayModeGetHeight(mode);
            //std::cout << std::endl << w << 'x' << h;
        }
#endif
    }
};
