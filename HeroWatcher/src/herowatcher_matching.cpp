// herowatcher_matching.cpp
// Combines shape‐based hero detection (Hu moments) with fallback template matching.
// Uses OBS’s dstr for all dynamic strings (no std::string).

#include "herowatcher_matching.h"
#include "herowatcher_matching.hpp"

#include <obs-module.h>
#include <util/dstr.h>
#include <util/bmem.h>
#include <util/platform.h>

#include <float.h>               // for DBL_MAX
#include <algorithm>             // for std::max_element

#include <vector>
#include <cstdint>               // for uint8_t
#include <cstring>               // for memcpy

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

// —————————————————————————————————————————————————
// Holds one hero’s name (OBS dstr) and its 7 Hu‐moments.
// —————————————————————————————————————————————————
struct HeroMoment {
    struct dstr name;
    cv::Mat      hu;   // 7×1 CV_64F
};

static HeroMoment  *heroMoments = nullptr;
static size_t       heroCount   = 0;
static bool         huLoaded    = false;

// —————————————————————————————————————————————————
// Load hero_hu_moments.json into heroMoments[]
// —————————————————————————————————————————————————
static void loadHeroMoments(const char *jsonPath)
{
    cv::FileStorage fs(jsonPath, cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    if (!fs.isOpened()) {
        blog(LOG_WARNING, "[%s] Cannot open JSON: %s", __func__, jsonPath);
        return;
    }

    cv::FileNode root = fs.root();
    heroCount = (size_t)root.size();
    heroMoments = (HeroMoment *)bzalloc(sizeof(*heroMoments) * heroCount);

    size_t idx = 0;
    for (cv::FileNodeIterator it = root.begin(); it != root.end(); ++it, ++idx) {
        cv::FileNode node = *it;
        // node.name() returns std::string, but no need to include <string> manually
        const char *heroName = node.name().c_str();

        dstr_init(&heroMoments[idx].name);
        dstr_copy(&heroMoments[idx].name, heroName);

        double vals[7];
        for (int i = 0; i < 7; i++)
            vals[i] = (double)node[i];
        heroMoments[idx].hu = cv::Mat(7, 1, CV_64F, vals).clone();
    }

    fs.release();
    blog(LOG_INFO, "[%s] Loaded %zu hero Hu moments", __func__, heroCount);
}

// —————————————————————————————————————————————————
// Compute L₂ distance between two 7‐element Hu vectors
// —————————————————————————————————————————————————
static double huDistance(const cv::Mat &a, const cv::Mat &b)
{
    return cv::norm(a, b, cv::NORM_L2);
}

// —————————————————————————————————————————————————
// Main detection: shape‐match first, then template‐match fallback
// —————————————————————————————————————————————————
bool do_template_match(const uint8_t *rgba_data, int width, int height, int linesize)
{
    blog(LOG_INFO, "[%s] Starting match (%dx%d)", __func__, width, height);

    // 1) Convert RGBA→GRAY
    cv::Mat rgba(height, width, CV_8UC4);
    for (int y = 0; y < height; y++)
        memcpy(rgba.ptr(y), rgba_data + y * linesize, width * 4);
    cv::Mat gray;
    cv::cvtColor(rgba, gray, cv::COLOR_RGBA2GRAY);

    // 2) Binarize + morphological closing
    cv::Mat bw;
    cv::threshold(gray, bw, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, {3, 3});
    cv::morphologyEx(bw, bw, cv::MORPH_CLOSE, kernel);

    // 3) Find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(bw, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (!contours.empty()) {
        // 4) Extract largest contour & compute Hu moments
        auto &largest = *std::max_element(contours.begin(), contours.end(),
            [](auto &a, auto &b){ return cv::contourArea(a) < cv::contourArea(b); });
        // compute Hu moments into a 7×1 CV_64F Mat
        cv::Moments m = cv::moments(largest);
        cv::Mat huInput(7, 1, CV_64F);
        cv::HuMoments(m, huInput);

        // 5) Lazy‐load hero moments JSON
        if (!huLoaded) {
            char *dir = obs_module_file("hero_images");
            if (dir) {
                char path[512];
                snprintf(path, sizeof(path), "%s/hero_hu_moments.json", dir);
                loadHeroMoments(path);
                bfree(dir);
            }
            huLoaded = true;
        }

        // 6) Compare against each hero’s Hu moments
        double bestDist = DBL_MAX;
        size_t bestIdx = SIZE_MAX;
        for (size_t i = 0; i < heroCount; i++) {
            double d = huDistance(huInput, heroMoments[i].hu);
            if (d < bestDist) {
                bestDist = d;
                bestIdx  = i;
            }
        }

        const double HU_THRESH = 0.1;  // tune via testing
        if (bestIdx != SIZE_MAX && bestDist < HU_THRESH) {
            blog(LOG_INFO, "[%s] Shape match → %s (dist=%.5f)",
                 __func__,
                 heroMoments[bestIdx].name.array,
                 bestDist);
            return true;
        } else {
            blog(LOG_INFO, "[%s] Shape uncertain (%.5f), falling back",
                 __func__, bestDist);
        }
    } else {
        blog(LOG_WARNING, "[%s] No contours found, skipping shape match", __func__);
    }

    // ————— TEMPLATE MATCHING FALLBACK —————
     cv::Mat matchSrc = gray;
    const int MIN_W = 520 * 2;
    const int MIN_H = 171 * 2;
    if (matchSrc.cols < MIN_W || matchSrc.rows < MIN_H) {
        cv::resize(matchSrc, matchSrc,
                   cv::Size(MIN_W, MIN_H),
                   0, 0, cv::INTER_LINEAR);
        blog(LOG_INFO, "[%s] Resized source for template matching to %dx%d",
             __func__, MIN_W, MIN_H);
    }
    char *template_dir = obs_module_file("hero_images");
    if (!template_dir) {
        blog(LOG_ERROR, "[%s] Failed to get hero_images path", __func__);
        return false;
    }

    struct dstr glob_pattern;
    dstr_init(&glob_pattern);
    dstr_printf(&glob_pattern, "%s/*.png", template_dir);
    bfree(template_dir);

    os_glob_t *glob = nullptr;
    if (os_glob(glob_pattern.array, 0, &glob) != 0 || !glob) {
        blog(LOG_WARNING, "[%s] os_glob failed: %s", __func__, glob_pattern.array);
        dstr_free(&glob_pattern);
        return false;
    }

    struct MatchResult {
        struct dstr filename;
        double        score;
        cv::Point     location;
    };
    std::vector<MatchResult> results;

    for (size_t i = 0; i < glob->gl_pathc; ++i) {
        auto &ent = glob->gl_pathv[i];
        if (ent.directory)
            continue;

        cv::Mat templ = cv::imread(ent.path, cv::IMREAD_GRAYSCALE);
        if (templ.empty()) {
            blog(LOG_WARNING, "[%s] Could not load %s", __func__, ent.path);
            continue;
        }
        if (templ.cols > gray.cols || templ.rows > gray.rows) {
            blog(LOG_WARNING, "[%s] Template too large: %s", __func__, ent.path);
            continue;
        }

        cv::Mat matchRes;
        cv::matchTemplate(matchSrc, templ, matchRes, cv::TM_CCOEFF_NORMED);

        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(matchRes, &minVal, &maxVal, &minLoc, &maxLoc);

        MatchResult mr;
        dstr_init(&mr.filename);
        dstr_copy(&mr.filename, ent.path);
        mr.score    = maxVal;
        mr.location = maxLoc;
        results.push_back(mr);
    }

    os_globfree(glob);
    dstr_free(&glob_pattern);

    if (results.empty()) {
        blog(LOG_INFO, "[%s] No template matches found", __func__);
        return false;
    }

    // Pick the highest score
    auto best = std::max_element(results.begin(), results.end(),
        [](const MatchResult &a, const MatchResult &b) {
            return a.score < b.score;
        });

    blog(LOG_INFO, "[%s] Template match → %s (score=%.3f)",
         __func__,
         best->filename.array,
         best->score);

    // Cleanup dstrs
    for (auto &r : results)
        dstr_free(&r.filename);

    return true;
}
