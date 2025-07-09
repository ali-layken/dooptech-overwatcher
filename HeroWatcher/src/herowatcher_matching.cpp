#include "herowatcher_matching.h"
#include "herowatcher_matching.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <obs-module.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <util/bmem.h>

struct MatchResult {
    struct dstr filename;
    double score;
    cv::Point location;
};

bool do_template_match(const uint8_t *rgba_data, int width, int height, int linesize)
{
    blog(LOG_INFO, "[%s] Starting OpenCV matching (%dx%d)", __func__, width, height);

    // Copy RGBA into OpenCV Mat
    cv::Mat rgba(height, width, CV_8UC4);
    for (int y = 0; y < height; ++y)
        memcpy(rgba.ptr(y), rgba_data + y * linesize, width * 4);

    // Convert to grayscale for matching
    cv::Mat gray;
    cv::cvtColor(rgba, gray, cv::COLOR_RGBA2GRAY);

        // Save the current frame for debugging
    if (!cv::imwrite("match_output_rgba.png", rgba)) {
        blog(LOG_ERROR, "[%s] Failed to save output image!", __func__);
    } else {
        blog(LOG_INFO, "[%s] Output saved to match_output.png", __func__);
    }

    // Resize if either dimension is smaller than required
    bool resized = false;
    int resized_width = gray.cols;
    int resized_height = gray.rows;

    if (gray.cols < 520) {
        resized_width = 1040;
        resized = true;
    }
    if (gray.rows < 171) {
        resized_height = 342;
        resized = true;
    }

    if (resized) {
        cv::resize(gray, gray, cv::Size(resized_width, resized_height), 0, 0, cv::INTER_LINEAR);
        blog(LOG_DEBUG, "[%s] Resized input to %dx%d for template matching", __func__, resized_width, resized_height);
        cv::imwrite("match_input_resized.png", gray);
    }

    // Save the current frame for debugging
    if (!cv::imwrite("match_output.png", gray)) {
        blog(LOG_ERROR, "[%s] Failed to save output image!", __func__);
    } else {
        blog(LOG_INFO, "[%s] Output saved to match_output.png", __func__);
    }

    char *template_folder_c = obs_module_file("hero_images");
    if (!template_folder_c) {
        blog(LOG_ERROR, "[%s] Failed to get module path for hero_images", __func__);
        return false;
    }

    struct dstr template_glob;
    dstr_init(&template_glob);
    dstr_printf(&template_glob, "%s/*.png", template_folder_c);
    bfree(template_folder_c);

    os_glob_t *glob = nullptr;
    if (os_glob(template_glob.array, 0, &glob) != 0 || !glob) {
        blog(LOG_WARNING, "[%s] Failed to glob hero_images: %s", __func__, template_glob.array);
        dstr_free(&template_glob);
        return false;
    }

    std::vector<MatchResult> results;

    for (size_t i = 0; i < glob->gl_pathc; ++i) {
        struct os_globent *ent = &glob->gl_pathv[i];
        if (ent->directory)
            continue;

        cv::Mat templ = cv::imread(ent->path, cv::IMREAD_GRAYSCALE);
        if (templ.empty()) {
            blog(LOG_WARNING, "[%s] Could not load template: %s", __func__, ent->path);
            continue;
        }

        if (templ.cols > gray.cols || templ.rows > gray.rows) {
            blog(LOG_WARNING, "[%s] Template %s is larger than frame, skipping", __func__, ent->path);
            continue;
        }

        cv::Mat result;
        cv::matchTemplate(gray, templ, result, cv::TM_CCOEFF_NORMED);

        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);

        MatchResult match;
        dstr_init_copy(&match.filename, ent->path);
        match.score = maxVal;
        match.location = maxLoc;
        results.push_back(match);
    }

    os_globfree(glob);
    dstr_free(&template_glob);

    if (results.empty()) {
        blog(LOG_INFO, "[%s] No templates matched.", __func__);
        return false;
    }

    auto best = std::max_element(results.begin(), results.end(),
        [](const MatchResult &a, const MatchResult &b) {
            return a.score < b.score;
        });

    blog(LOG_INFO, "[%s] Best match: %s (score: %.3f)", __func__,
         best->filename.array, best->score);


    for (auto &r : results)
        dstr_free(&r.filename);

    return true;
}
