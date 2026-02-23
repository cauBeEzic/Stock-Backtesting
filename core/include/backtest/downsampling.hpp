#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace stockbt {

struct SeriesPoint {
    int64_t ts{0};
    double value{0.0};
};

struct BucketMinMax {
    int64_t min_ts{0};
    double min_value{0.0};
    int64_t max_ts{0};
    double max_value{0.0};
};

std::vector<BucketMinMax> downsample_bucket_min_max(const std::vector<SeriesPoint>& points,
                                                     std::size_t pixel_width,
                                                     std::size_t display_cap = 50000);

} // namespace stockbt
