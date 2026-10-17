#pragma once

namespace dpm
{

/** Installs the UI translation for the system language, once per process.
    Strings in the code are English and go through TRANS(); a language
    without a translation stays English.
*/
void installTranslations();

} // namespace dpm
