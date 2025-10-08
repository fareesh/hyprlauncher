#include "UnicodeFinder.hpp"
#include "../../helpers/Log.hpp"
#include "../Fuzzy.hpp"

#include <algorithm>
#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/utypes.h>

#include <hyprutils/os/Process.hpp>

using namespace Hyprutils::OS;

class CUnicodeEntry : public IFinderResult {
  public:
    CUnicodeEntry()          = default;
    virtual ~CUnicodeEntry() = default;

    virtual std::string fuzzable() {
        return m_fuzzable;
    }

    virtual eFinderTypes type() {
        return FINDER_UNICODE;
    }

    virtual void run() {
        Debug::log(TRACE, "Copying {} with wl-copy", m_unicode);

        CProcess proc("wl-copy", {m_unicode});
        proc.runAsync();
    }

    std::string m_name, m_unicode, m_fuzzable;
};

static bool isSurrogate(UChar32 cp) {
    return cp >= 0xD800 && cp <= 0xDFFF;
}

CUnicodeFinder::CUnicodeFinder() {
    for (UChar32 cp = 0; cp <= 0x10FFFF; ++cp) {
        if (isSurrogate(cp))
            continue;

        UErrorCode        status = U_ZERO_ERROR;
        char              nameBuf[256];
        int32_t           len = u_charName(cp, U_EXTENDED_CHAR_NAME, nameBuf, sizeof(nameBuf), &status);

        std::vector<char> dynBuf;
        if (status == U_BUFFER_OVERFLOW_ERROR && len > 0) {
            dynBuf.resize(len + 1);
            status = U_ZERO_ERROR;
            len    = u_charName(cp, U_EXTENDED_CHAR_NAME, dynBuf.data(), (int32_t)dynBuf.size(), &status);
        }

        if (U_FAILURE(status) || len <= 0)
            continue;

        const char*        name = (dynBuf.empty() ? nameBuf : dynBuf.data());

        icu::UnicodeString us;
        us.append(cp);
        std::string utf8;
        us.toUTF8String(utf8);

        auto& e       = m_unicodeEntryCache.emplace_back(makeShared<CUnicodeEntry>());
        e->m_unicode  = utf8;
        e->m_name     = name;
        e->m_fuzzable = name;
        std::ranges::transform(e->m_fuzzable, e->m_fuzzable.begin(), ::tolower);
        std::ranges::transform(e->m_name, e->m_name.begin(), ::toupper);
        m_unicodeEntryCacheGeneric.emplace_back(e);
    }
}

std::vector<SFinderResult> CUnicodeFinder::getResultsForQuery(const std::string& query) {
    std::vector<SFinderResult> results;

    auto                       fuzzed = Fuzzy::getNResults(m_unicodeEntryCacheGeneric, query, MAX_RESULTS_PER_FINDER);

    results.reserve(fuzzed.size());

    for (const auto& f : fuzzed) {
        if (!f)
            continue;
        const auto p = reinterpretPointerCast<CUnicodeEntry>(f);
        results.emplace_back(SFinderResult{
            .label  = p->m_unicode + " -> " + p->m_name,
            .icon   = "",
            .result = p,
        });
    }

    return results;
}
