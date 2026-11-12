#ifndef MAI_IR_LINGUISTICS_STATISTICS_HPP
#define MAI_IR_LINGUISTICS_STATISTICS_HPP

#include <cstddef>
#include <cstdint>

struct MandelbrotFit {
    double c = 0.0;
    double b = 0.0;
    double s = 0.0;
    double rmse = 0.0;
};

MandelbrotFit fit_mandelbrot(const uint64_t *frequencies, size_t size,
                             size_t fit_terms);
void write_zipf_svg(const char *path, const uint64_t *frequencies,
                    size_t size, const MandelbrotFit *fit);

#endif
