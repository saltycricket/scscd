#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// ---------- Small hashing utils ----------
static inline uint64_t fnv1a64(std::string_view s) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
    return h;
}
static inline uint32_t murmur32(std::string_view s, uint32_t seed = 0x9747b28c) {
    // Very small Murmur3-ish mix for bucketing (not crypto)
    uint32_t h = seed ^ static_cast<uint32_t>(s.size());
    for (unsigned char c : s) {
        uint32_t k = c;
        k *= 0xcc9e2d51u; k = std::rotl(k, 15); k *= 0x1b873593u;
        h ^= k; h = std::rotl(h, 13); h = h * 5 + 0xe6546b64u;
    }
    h ^= h >> 16; h *= 0x85ebca6bu; h ^= h >> 13; h *= 0xc2b2ae35u; h ^= h >> 16;
    return h;
}

// ---------- Normalization / tokenization ----------
struct Norm {
    // stopwords & suffixes you can tweak
    static const std::unordered_set<std::string>& Stop() {
        static const std::unordered_set<std::string> k{
            "male","female","m","f","man","woman","left","right","l","r",
            "skin","1st","3rd","first","third",
            "small","medium","large","s","m","l","xl","xxl",
            "v1","v2","v3","alt","a","b","c","var",
            "red","blue","green","black","white","brown","tan","gray","grey","dark","light",
            "shirt","pants","boots","hat","gloves","coat","hood","helmet","armor","torso","legs",
            "dn","arma","mswp","omod","aaa","bbb"
        };
        return k;
    }
    static bool isNumeric(std::string_view t) {
        if (t.empty()) return false;
        for (unsigned char c : t) if (!std::isdigit(c)) return false;
        return true;
    }

    // split CamelCase and non-alnum to tokens, lowercase
    static std::vector<std::string> tokens(std::string_view edid) {
        std::string s; s.reserve(edid.size() * 2);
        // replace non-alnum with space, split camel
        auto flushUpperSeq = [&](std::string& buff, unsigned char c) {
            // if boundary between lower/digit and upper, add space
            if (!buff.empty() && (std::islower(static_cast<unsigned char>(buff.back())) || std::isdigit(static_cast<unsigned char>(buff.back()))))
                buff.push_back(' ');
            buff.push_back(std::tolower(c));
            };
        for (size_t i = 0; i < edid.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(edid[i]);
            if (std::isalnum(c)) {
                if (std::isupper(c)) flushUpperSeq(s, c);
                else s.push_back(std::tolower(c));
            }
            else {
                s.push_back(' ');
            }
        }
        // collapse spaces
        std::vector<std::string> out;
        std::string cur;
        for (unsigned char c : s) {
            if (c == ' ') {
                if (!cur.empty()) { out.push_back(cur); cur.clear(); }
            }
            else cur.push_back(c);
        }
        if (!cur.empty()) out.push_back(cur);
        return out;
    }

    // strip trailing noise and return core prefix (first 2-4 tokens)
    static std::string coreKey(const std::vector<std::string>& toks) {
        if (toks.empty()) return {};
        int end = static_cast<int>(toks.size()) - 1;
        // trim from end: numeric or stop words
        while (end >= 0) {
            const auto& t = toks[end];
            if (isNumeric(t) || Stop().count(t)) --end;
            else break;
        }
        if (end < 0) return {};
        // also drop trailing generic garment nouns if they appear at end
        int start = 0;
        int used = std::min(4, end - start + 1);
        if (used >= 2) used = std::max(2, used); // prefer 2+
        std::string key;
        for (int i = 0; i < used; ++i) {
            if (i) key.push_back(' ');
            key += toks[i];
        }
        return key;
    }

    // join tokens to a single cleaned string for n-grams
    static std::string cleanedJoin(const std::vector<std::string>& toks) {
        std::string j;
        size_t total = 0; for (auto& t : toks) total += t.size() + 1;
        j.reserve(total);
        for (size_t i = 0; i < toks.size(); ++i) {
            if (i) j.push_back(' ');
            j += toks[i];
        }
        return j;
    }
};

// ---------- Character trigram features ----------
static inline void trigrams(std::string_view s, std::vector<uint32_t>& out) {
    out.clear();
    if (s.size() < 3) {
        if (!s.empty()) out.push_back(murmur32(s));
        return;
    }
    for (size_t i = 0; i + 2 < s.size(); ++i) {
        char a = s[i], b = s[i + 1], c = s[i + 2];
        // skip spaces-only grams
        if (a == ' ' && b == ' ' && c == ' ') continue;
        char gram[3] = { a,b,c };
        out.push_back(murmur32(std::string_view{ gram,3 }));
    }
}

// ---------- 16D hashed TF-IDF projection ----------
struct Projector16 {
    // build-time DF table
    std::unordered_map<uint32_t, uint32_t> df;
    uint32_t N_docs = 0;

    void observeDoc(const std::vector<uint32_t>& feats) {
        ++N_docs;
        // unique features per doc
        std::unordered_set<uint32_t> seen;
        seen.reserve(feats.size());
        for (auto f : feats) seen.insert(f);
        for (auto f : seen) df[f]++;
    }

    // Compute 16D TF-IDF hashed projection (sign from hash)
    std::array<float, 16> project(const std::vector<uint32_t>& feats) const {
        std::array<float, 16> v{}; v.fill(0.f);
        if (feats.empty()) return v;
        // term frequencies
        std::unordered_map<uint32_t, uint32_t> tf;
        tf.reserve(feats.size());
        for (auto f : feats) tf[f]++;

        auto idf = [&](uint32_t f)->float {
            auto it = df.find(f);
            uint32_t dfi = (it == df.end() ? 1u : it->second);
            // smoothed
            return std::log((static_cast<float>(N_docs) + 1.f) / (static_cast<float>(dfi) + 1.f)) + 1.f;
            };

        for (auto& [f, cnt] : tf) {
            float w = static_cast<float>(cnt) * idf(f);
            uint32_t h = f; // already hashed
            uint32_t idx = h & 15u;         // 0..15
            bool sign = ((h >> 5) & 1u);    // pseudo random sign
            v[idx] += sign ? w : -w;
        }
        // L2 normalize
        float n2 = 0.f; for (float x : v) n2 += x * x;
        if (n2 > 0.f) {
            float inv = 1.f / std::sqrt(n2);
            for (float& x : v) x *= inv;
        }
        return v;
    }

    static float cosine(const std::array<float, 16>& a, const std::array<float, 16>& b) {
        float s = 0.f; for (int i = 0; i < 16; ++i) s += a[i] * b[i];
        // a,b are normalized, so s in [-1,1], clamp to [0,1]
        return std::max(0.f, s);
    }
};

// ---------- 64-bit SimHash ----------
static inline uint64_t simhash64(const std::vector<uint32_t>& feats,
    const Projector16& proj) {
    // weight feats with (tf * idf)
    std::array<double, 64> acc{}; acc.fill(0.0);
    std::unordered_map<uint32_t, uint32_t> tf;
    for (auto f : feats) tf[f]++;

    for (auto& [f, cnt] : tf) {
        float idf = std::log((proj.N_docs + 1.f) / ((proj.df.contains(f) ? proj.df.at(f) : 1u) + 1.f)) + 1.f;
        double w = static_cast<double>(cnt) * static_cast<double>(idf);
        uint64_t h = static_cast<uint64_t>(f) * 0x9E3779B97F4A7C15ull; // spread
        for (int b = 0; b < 64; ++b) {
            acc[b] += ((h >> b) & 1ull) ? w : -w;
        }
    }
    uint64_t out = 0;
    for (int b = 0; b < 64; ++b) if (acc[b] > 0.0) out |= (1ull << b);
    return out;
}

static inline int popcount64(uint64_t x) { return std::popcount(x); }

// ---------- Main index ----------
struct ItemVec {
    uint32_t formID{};
    std::string edid;
    std::vector<std::string> toks;
    std::string core;
    uint64_t coreHash{};
    std::vector<uint32_t> features;      // trigrams
    std::array<float, 16> proj{};         // 16D normalized
    uint64_t simhash{};
};

class EdidIndex {
public:
    // weights (tweak if desired)
    float w_core = 0.60f;
    float w_cos = 0.20f;
    float w_sim = 0.15f;
    float w_edit = 0.05f; // reserved (Jaro-Winkler) ' not implemented to keep compact

    // ' for simhash Hamming decay
    float tau = 8.0f;

    void clear() {
        items_.clear();
        byForm_.clear();
        byCore_.clear();
        proj_ = Projector16{};
    }

    void reserve(size_t n) { items_.reserve(n); }

    // Add one form's EDID (call before Finalize)
    void add(uint32_t formID, std::string_view edid) {
        ItemVec it;
        it.formID = formID;
        it.edid = std::string(edid);
        it.toks = Norm::tokens(it.edid);
        it.core = Norm::coreKey(it.toks);
        it.coreHash = fnv1a64(it.core);
        std::string joined = Norm::cleanedJoin(it.toks);
        trigrams(joined, it.features);

        // observe DF
        proj_.observeDoc(it.features);

        byForm_[formID] = static_cast<uint32_t>(items_.size());
        items_.push_back(std::move(it));
    }

    // Build projections and simhashes (call once after all add())
    void finalize() {
        for (auto& it : items_) {
            it.proj = proj_.project(it.features);
            it.simhash = simhash64(it.features, proj_);
            byCore_[it.coreHash].push_back(it.formID);
        }
        finalized_ = true;
    }

    // Similarity in [0,1]
    float similarity(uint32_t aForm, uint32_t bForm) const {
        auto ia = get(aForm), ib = get(bForm);
        if (!ia || !ib) return 0.0f;
        const auto& A = items_[*ia];
        const auto& B = items_[*ib];

        float s_core = (A.coreHash == B.coreHash) ? 1.0f : 0.0f;
        float s_cos = Projector16::cosine(A.proj, B.proj);
        int   ham = popcount64(A.simhash ^ B.simhash);
        float s_sim = std::exp(-static_cast<float>(ham) / tau);

        float s = w_core * s_core + w_cos * s_cos + w_sim * s_sim /* + w_edit * jw */;
        if (s < 0.f) s = 0.f; else if (s > 1.f) s = 1.f;
        return s;
    }

    // Pick from candidates with proximity to 'seed' (returns formID or 0 if empty)
    uint32_t sampleBiased(uint32_t seedForm,
        const std::vector<uint32_t>& candidates,
        float beta = 2.0f,
        uint32_t rndSeed = 0xC0FFEEu) const
    {
        if (candidates.empty()) return 0;
        auto is = get(seedForm);
        // if no seed, choose at random.
        if (!is) return candidates[rand() % candidates.size()];
        std::vector<double> weights; weights.reserve(candidates.size());
        double total = 0.0;
        for (auto c : candidates) {
            float s = similarity(seedForm, c);
            double w = std::exp(static_cast<double>(beta) * static_cast<double>(s));
            weights.push_back(w); total += w;
        }
        if (total <= 0.0) return candidates[0];
        // roulette wheel
        std::mt19937_64 rng{ static_cast<uint64_t>(rndSeed) ^ (static_cast<uint64_t>(seedForm) << 1) };
        std::uniform_real_distribution<double> U(0.0, total);
        double r = U(rng);
        double acc = 0.0;
        for (size_t i = 0; i < candidates.size(); ++i) {
            acc += weights[i];
            if (r <= acc) return candidates[i];
        }
        return candidates.back();
    }

    // Quick accessors
    const std::vector<uint32_t>& coreBucket(uint64_t coreHash) const {
        static const std::vector<uint32_t> kEmpty;
        auto it = byCore_.find(coreHash);
        return it == byCore_.end() ? kEmpty : it->second;
    }
    uint64_t coreHashOf(uint32_t formID) const {
        auto i = get(formID); return i ? items_[*i].coreHash : 0ull;
    }
    const std::string& coreKeyOf(uint32_t formID) const {
        static const std::string k{};
        auto i = get(formID); return i ? items_[*i].core : k;
    }

private:
    std::optional<uint32_t> get(uint32_t formID) const {
        auto it = byForm_.find(formID);
        if (it == byForm_.end()) return std::nullopt;
        return it->second;
    }

    bool finalized_ = false;
    Projector16 proj_{};
    std::vector<ItemVec> items_;
    std::unordered_map<uint32_t, uint32_t> byForm_;
    std::unordered_map<uint64_t, std::vector<uint32_t>> byCore_;
};
