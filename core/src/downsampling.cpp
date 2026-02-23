#include "backtest/downsampling.hpp"

#include <algorithm>

namespace stockbt {

std::vector<BucketMinMax> downsample_bucket_min_max(const std::vector<SeriesPoint>& points,
                                                     std::size_t pixel_width,
                                                     std::size_t display_cap) {
    if (points.empty() || pixel_width == 0) {
        return {};
    }

    if (points.size() <= display_cap) {
        std::vector<BucketMinMax> out;
        out.reserve(points.size());
        for (const SeriesPoint& p : points) {
            out.push_back({p.ts, p.value, p.ts, p.value});
        }
        return out;
    }

    const std::size_t bucket_count = std::max<std::size_t>(1, std::min(pixel_width, display_cap / 2));
    std::vector<BucketMinMax> out;
    out.reserve(bucket_count);

    for (std::size_t b = 0; b < bucket_count; ++b) {
        const std::size_t start = (b * points.size()) / bucket_count;
        const std::size_t end = ((b + 1) * points.size()) / bucket_count;
        if (start >= end) {
            continue;
        }

        std::size_t min_idx = start;
        std::size_t max_idx = start;
        for (std::size_t i = start + 1; i < end; ++i) {
            if (points[i].value < points[min_idx].value) {
                min_idx = i;
            }
            if (points[i].value > points[max_idx].value) {
                max_idx = i;
            }
        }

        out.push_back({points[min_idx].ts, points[min_idx].value, points[max_idx].ts, points[max_idx].value});
    }

    return out;
}

} // namespace stockbt
