#include "HostWindowTitle.h"

#import <AppKit/AppKit.h>

namespace dpm
{

juce::String readHostWindowTitle()
{
    @autoreleasepool
    {
        NSWindow* best = nil;
        CGFloat bestArea = 0;

        for (NSWindow* window in [NSApp windows])
        {
            if (! [window isVisible] || [[window title] length] == 0)
                continue;
            if (isOwnWindowTitle (juce::String::fromUTF8 ([[window title] UTF8String])))
                continue;

            const auto frame = [window frame];
            const auto area = frame.size.width * frame.size.height;
            if (area > bestArea)
            {
                bestArea = area;
                best = window;
            }
        }

        if (best == nil)
            return {};

        auto title = juce::String::fromUTF8 ([[best title] UTF8String]);

        // Document-based hosts (Logic, GarageBand, …) name the project file
        // directly; with it the full path can match.
        if (NSString* represented = [best representedFilename]; [represented length] > 0)
            title << " [" << juce::String::fromUTF8 ([represented UTF8String]) << "]";

        return title;
    }
}

} // namespace dpm
