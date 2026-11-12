#include <gtest/gtest.h>

#include "linguistics/reporting/statistics.hpp"

#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {

bool file_contains(const char *path, const char *needle) {
    FILE *file = std::fopen(path, "rb");
    if (file == nullptr) return false;

    char line[4096];
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        if (std::strstr(line, needle) != nullptr) {
            std::fclose(file);
            return true;
        }
    }
    std::fclose(file);
    return false;
}

class TemporaryFile {
public:
    TemporaryFile() {
        std::strcpy(path_, "/tmp/mai-ir-zipf-XXXXXX");
        const int descriptor = mkstemp(path_);
        if (descriptor >= 0) {
            close(descriptor);
        } else {
            path_[0] = '\0';
        }
    }

    ~TemporaryFile() { unlink(path_); }

    TemporaryFile(const TemporaryFile &) = delete;
    TemporaryFile &operator=(const TemporaryFile &) = delete;

    bool valid() const { return path_[0] != '\0'; }
    const char *path() const { return path_; }

private:
    char path_[64]{};
};

constexpr uint64_t SAMPLE_FREQUENCIES[] = {1000, 520, 340, 250, 205, 170};

// Проверяется подбор положительных параметров модели Ципфа–Мандельброта.
TEST(DocumentProcessingTest, FitsMandelbrotDistributionToTermFrequencies) {
    const MandelbrotFit fit = fit_mandelbrot(SAMPLE_FREQUENCIES, 6, 6);

    EXPECT_GT(fit.c, 0.0);
    EXPECT_GE(fit.b, 0.0);
    EXPECT_GT(fit.s, 0.0);
    EXPECT_GE(fit.rmse, 0.0);
}

// Проверяется создание SVG с подписями, единицами и логарифмическими делениями.
TEST(DocumentProcessingTest, WritesReadableZipfSvgWithUnitsAndLogarithmicTicks) {
    const MandelbrotFit fit = fit_mandelbrot(SAMPLE_FREQUENCIES, 6, 6);
    const TemporaryFile output;
    ASSERT_TRUE(output.valid());

    write_zipf_svg(output.path(), SAMPLE_FREQUENCIES, 6, &fit);

    EXPECT_TRUE(file_contains(output.path(),
                              "Ранг терма r [порядковый номер]"));
    EXPECT_TRUE(file_contains(output.path(),
                              "Частота f(r) [число вхождений]"));
    EXPECT_TRUE(file_contains(output.path(), ">10^2</text>"));
    EXPECT_TRUE(file_contains(output.path(), "Мандельброт"));
}

}  // namespace
