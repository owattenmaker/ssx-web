// Side by side for the visual-parity checks (docs/native.md "N3"): web | native | the difference (x4), and the mean / 95th percentile
// absolute channel difference. The two images must be the same size.
//   ssx3_image_compare WEB.png NATIVE.png OUT.png
#include "image_io.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: ssx3_image_compare WEB.png NATIVE.png OUT.png\n");
        return 2;
    }
    const auto a = ssx_native::readPng(argv[1]);
    const auto b = ssx_native::readPng(argv[2]);
    if (a.width != b.width || a.height != b.height) {
        std::fprintf(stderr, "sizes differ: %dx%d and %dx%d\n", a.width, a.height, b.width, b.height);
        return 1;
    }
    const int w = a.width;
    const int h = a.height;
    std::vector<uint8_t> out(size_t(w) * 3 * h * 4, 255);
    std::vector<int> diffs;
    diffs.reserve(size_t(w) * h * 3);
    double sum = 0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const size_t i = (size_t(y) * w + x) * 4;
            for (int c = 0; c < 4; ++c) {
                out[(size_t(y) * w * 3 + x) * 4 + c] = a.rgba[i + c];
                out[(size_t(y) * w * 3 + w + x) * 4 + c] = b.rgba[i + c];
            }
            for (int c = 0; c < 3; ++c) {
                const int d = std::abs(int(a.rgba[i + c]) - int(b.rgba[i + c]));
                diffs.push_back(d);
                sum += d;
                out[(size_t(y) * w * 3 + 2 * w + x) * 4 + c] = uint8_t(std::min(255, d * 4));
            }
        }
    }
    std::sort(diffs.begin(), diffs.end());
    ssx_native::writePng(argv[3], w * 3, h, out.data());
    std::printf("{\"mean\":%.3f,\"p95\":%d,\"same\":%.4f}\n", sum / double(diffs.size()), diffs[diffs.size() * 95 / 100],
                double(std::count(diffs.begin(), diffs.end(), 0)) / double(diffs.size()));
    return 0;
}
