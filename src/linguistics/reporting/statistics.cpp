#include "linguistics/reporting/statistics.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

double plot_x(size_t rank, size_t maximum, double left, double width) {
    return left + std::log10(static_cast<double>(rank)) /
                      std::log10(static_cast<double>(maximum)) * width;
}

double plot_y(double frequency, double maximum, double top, double height) {
    const double normalized = std::log10(frequency) / std::log10(maximum);
    return top + (1.0 - normalized) * height;
}

void write_log_tick_label(FILE *file, double x, double y, size_t exponent,
                          const char *anchor) {
    if (exponent == 0) {
        std::fprintf(file,
                     "<text x=\"%.2f\" y=\"%.2f\" text-anchor=\"%s\" "
                     "font-family=\"sans-serif\" font-size=\"11\">1</text>\n",
                     x, y, anchor);
        return;
    }
    std::fprintf(file,
                 "<text x=\"%.2f\" y=\"%.2f\" text-anchor=\"%s\" "
                 "font-family=\"sans-serif\" font-size=\"11\">10^%zu</text>\n",
                 x, y, anchor, exponent);
}

void write_plot_axes(FILE *file, size_t size, double maximum) {
    constexpr double LEFT = 80.0;
    constexpr double TOP = 35.0;
    constexpr double WIDTH = 880.0;
    constexpr double HEIGHT = 500.0;
    size_t rank = 1;
    for (size_t exponent = 0; rank <= size; ++exponent) {
        const double x = plot_x(rank, size, LEFT, WIDTH);
        std::fprintf(file,
                     "<line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" "
                     "stroke=\"#d1d5db\" stroke-width=\"1\"/>\n"
                     "<line x1=\"%.2f\" y1=\"535\" x2=\"%.2f\" y2=\"541\" "
                     "stroke=\"black\"/>\n",
                     x, TOP, x, TOP + HEIGHT, x, x);
        write_log_tick_label(file, x, 554.0, exponent, "middle");
        if (rank > size / 10) break;
        rank *= 10;
    }

    double frequency = 1.0;
    for (size_t exponent = 0; frequency <= maximum; ++exponent) {
        const double y = plot_y(frequency, maximum, TOP, HEIGHT);
        std::fprintf(file,
                     "<line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" "
                     "stroke=\"#d1d5db\" stroke-width=\"1\"/>\n"
                     "<line x1=\"74\" y1=\"%.2f\" x2=\"80\" y2=\"%.2f\" "
                     "stroke=\"black\"/>\n",
                     LEFT, y, LEFT + WIDTH, y, y, y);
        write_log_tick_label(file, 69.0, y + 4.0, exponent, "end");
        if (frequency > maximum / 10.0) break;
        frequency *= 10.0;
    }
    std::fputs(
        "<line x1=\"80\" y1=\"35\" x2=\"80\" y2=\"535\" "
        "stroke=\"black\" stroke-width=\"1.5\"/>\n"
        "<line x1=\"80\" y1=\"535\" x2=\"960\" y2=\"535\" "
        "stroke=\"black\" stroke-width=\"1.5\"/>\n",
        file);
}

void write_polyline(FILE *file, const uint64_t *frequencies, size_t size,
                    const MandelbrotFit *fit, int mode) {
    constexpr double LEFT = 80.0;
    constexpr double TOP = 35.0;
    constexpr double WIDTH = 880.0;
    constexpr double HEIGHT = 500.0;
    const double maximum = static_cast<double>(frequencies[0]);
    const size_t points = size < 1600 ? size : 1600;
    const char *color =
        mode == 0 ? "#2563eb" : (mode == 1 ? "#dc2626" : "#16a34a");
    std::fprintf(file,
                 "<polyline fill=\"none\" stroke=\"%s\" stroke-width=\"1.5\" "
                 "points=\"",
                 color);
    for (size_t point = 0; point < points; ++point) {
        const size_t rank = 1 + point * (size - 1) / (points - 1);
        double frequency = static_cast<double>(frequencies[rank - 1]);
        if (mode == 1) frequency = maximum / static_cast<double>(rank);
        if (mode == 2) {
            frequency = fit->c /
                        std::pow(static_cast<double>(rank) + fit->b, fit->s);
        }
        if (frequency < 1.0) frequency = 1.0;
        std::fprintf(file, "%.2f,%.2f ",
                     plot_x(rank, size, LEFT, WIDTH),
                     plot_y(frequency, maximum, TOP, HEIGHT));
    }
    std::fputs("\"/>\n", file);
}

}  // namespace

MandelbrotFit fit_mandelbrot(const uint64_t *frequencies, size_t size,
                             size_t fit_terms) {
    MandelbrotFit best{};
    best.rmse = HUGE_VAL;
    if (size > fit_terms) size = fit_terms;
    if (size < 2) return {};
    for (size_t candidate = 0; candidate <= 100; ++candidate) {
        const double b = candidate == 0
                             ? 0.0
                             : std::pow(10.0, -2.0 + 6.0 *
                                                       static_cast<double>(candidate - 1) /
                                                       99.0);
        double sum_x = 0.0;
        double sum_y = 0.0;
        double sum_xx = 0.0;
        double sum_xy = 0.0;
        for (size_t index = 0; index < size; ++index) {
            const double x = std::log(static_cast<double>(index + 1) + b);
            const double y = std::log(static_cast<double>(frequencies[index]));
            sum_x += x;
            sum_y += y;
            sum_xx += x * x;
            sum_xy += x * y;
        }
        const double count = static_cast<double>(size);
        const double denominator = count * sum_xx - sum_x * sum_x;
        if (denominator == 0.0) continue;
        const double slope = (count * sum_xy - sum_x * sum_y) / denominator;
        const double intercept = (sum_y - slope * sum_x) / count;
        double squared_error = 0.0;
        for (size_t index = 0; index < size; ++index) {
            const double predicted =
                intercept + slope *
                                std::log(static_cast<double>(index + 1) + b);
            const double error =
                std::log(static_cast<double>(frequencies[index])) - predicted;
            squared_error += error * error;
        }
        const double rmse = std::sqrt(squared_error / count);
        if (rmse < best.rmse) {
            best = {std::exp(intercept), b, -slope, rmse};
        }
    }
    return best;
}

void write_zipf_svg(const char *path, const uint64_t *frequencies,
                    size_t size, const MandelbrotFit *fit) {
    if (size < 2) return;
    FILE *file = std::fopen(path, "wb");
    if (file == nullptr) {
        std::fprintf(stderr, "linguistics: не удалось записать %s\n", path);
        std::exit(EXIT_FAILURE);
    }
    std::fputs(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1000\" "
        "height=\"650\" viewBox=\"0 0 1000 650\">\n"
        "<rect width=\"1000\" height=\"650\" fill=\"white\"/>\n"
        "<text x=\"500\" y=\"22\" text-anchor=\"middle\" "
        "font-family=\"sans-serif\" font-size=\"18\">Распределение "
        "терминов по частотности</text>\n",
        file);
    write_plot_axes(file, size, static_cast<double>(frequencies[0]));
    write_polyline(file, frequencies, size, fit, 0);
    write_polyline(file, frequencies, size, fit, 1);
    write_polyline(file, frequencies, size, fit, 2);
    std::fputs(
        "<line x1=\"270\" y1=\"580\" x2=\"305\" y2=\"580\" stroke=\"#2563eb\" stroke-width=\"2\"/><text x=\"312\" y=\"584\" font-family=\"sans-serif\" font-size=\"12\">корпус</text>\n"
        "<line x1=\"405\" y1=\"580\" x2=\"440\" y2=\"580\" stroke=\"#dc2626\" stroke-width=\"2\"/><text x=\"447\" y=\"584\" font-family=\"sans-serif\" font-size=\"12\">Ципф</text>\n"
        "<line x1=\"530\" y1=\"580\" x2=\"565\" y2=\"580\" stroke=\"#16a34a\" stroke-width=\"2\"/><text x=\"572\" y=\"584\" font-family=\"sans-serif\" font-size=\"12\">Мандельброт</text>\n"
        "<text x=\"520\" y=\"625\" text-anchor=\"middle\" font-family=\"sans-serif\" font-size=\"13\">Ранг терма r [порядковый номер], шкала log10</text>\n"
        "<text x=\"15\" y=\"285\" transform=\"rotate(-90 15 285)\" text-anchor=\"middle\" font-family=\"sans-serif\" font-size=\"13\">Частота f(r) [число вхождений], шкала log10</text>\n</svg>\n",
        file);
    std::fclose(file);
}
