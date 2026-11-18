// chiaki-fgbench: measures the quality and GPU time of Flow frame generation.
//
// Takes a folder of consecutive PNG frames of a 60 fps video, keeps every
// second one (a 30 fps source, like a 30 fps game) and lets Flow rebuild the
// dropped frames (t = 0.5). Each rebuilt frame is compared with the real one
// (PSNR of luma, whole frame and the 5% worst 16x16 blocks, where artifacts
// show). Two baselines are printed: repeating the previous frame and a plain
// 50/50 blend. With --step 4 the source is 15 fps and t = 0.25/0.5/0.75 are
// rebuilt (harder, larger motion, like x4 output).
//
// Usage: chiaki-fgbench <frames_dir> [--step 2|4] [--scale 4|2] [--max N]
//                       [--dump out_dir] [--dump-count N]

#include "flowfg.h"

#include <libplacebo/gpu.h>
#include <libplacebo/log.h>
#include <libplacebo/vulkan.h>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QLoggingCategory>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

Q_LOGGING_CATEGORY(chiakiGui, "chiaki.gui", QtWarningMsg)

namespace {

struct Score
{
    double mse_sum = 0.0;
    double worst_sum = 0.0;
    int count = 0;
    void add(double mse, double worst)
    {
        mse_sum += mse;
        worst_sum += worst;
        count++;
    }
    static double psnr(double mse) { return mse <= 1e-12 ? 99.0 : 10.0 * std::log10(1.0 / mse); }
    double meanPsnr() const { return count ? psnr(mse_sum / count) : 0.0; }
    double worstPsnr() const { return count ? psnr(worst_sum / count) : 0.0; }
};

double luma(const uchar *p) { return (0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2]) / 255.0; }

// MSE of luma over the frame and over its worst 5% of 16x16 blocks.
void compare(const QImage &a, const QImage &b, double &mse, double &worst)
{
    const int w = a.width(), h = a.height();
    const int bw = (w + 15) / 16, bh = (h + 15) / 16;
    std::vector<double> blocks(static_cast<size_t>(bw * bh), 0.0);
    std::vector<int> counts(blocks.size(), 0);
    double total = 0.0;
    for (int y = 0; y < h; y++) {
        const uchar *ra = a.constScanLine(y);
        const uchar *rb = b.constScanLine(y);
        for (int x = 0; x < w; x++) {
            const double d = luma(ra + 4 * x) - luma(rb + 4 * x);
            const size_t bi = static_cast<size_t>((y / 16) * bw + x / 16);
            blocks[bi] += d * d;
            counts[bi]++;
            total += d * d;
        }
    }
    for (size_t i = 0; i < blocks.size(); i++)
        blocks[i] /= std::max(counts[i], 1);
    std::sort(blocks.begin(), blocks.end(), std::greater<double>());
    const size_t n = std::max<size_t>(1, blocks.size() / 20);
    double ws = 0.0;
    for (size_t i = 0; i < n; i++)
        ws += blocks[i];
    mse = total / (static_cast<double>(w) * h);
    worst = ws / n;
}

QImage blend(const QImage &a, const QImage &b, double t)
{
    QImage out(a.size(), QImage::Format_RGBA8888);
    for (int y = 0; y < a.height(); y++) {
        const uchar *ra = a.constScanLine(y);
        const uchar *rb = b.constScanLine(y);
        uchar *ro = out.scanLine(y);
        for (int x = 0; x < a.width() * 4; x++)
            ro[x] = static_cast<uchar>(std::lround(ra[x] * (1.0 - t) + rb[x] * t));
    }
    return out;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() < 2) {
        std::fprintf(stderr, "usage: chiaki-fgbench <frames_dir> [--step 2|4] [--scale 4|2] [--max N] [--dump dir] [--dump-count N]\n");
        return 2;
    }
    int step = 2, scale = 4, max_pairs = 1000000, dump_count = 6;
    QString dump_dir;
    for (int i = 2; i < args.size(); i++) {
        const QString &a = args[i];
        const QString v = i + 1 < args.size() ? args[i + 1] : QString();
        if (a == "--step") { step = v.toInt() == 4 ? 4 : 2; i++; }
        else if (a == "--scale") { scale = v.toInt() == 2 ? 2 : 4; i++; }
        else if (a == "--max") { max_pairs = std::max(1, v.toInt()); i++; }
        else if (a == "--dump") { dump_dir = v; i++; }
        else if (a == "--dump-count") { dump_count = v.toInt(); i++; }
    }
    QDir dir(args[1]);
    const QStringList files = dir.entryList({ "*.png" }, QDir::Files, QDir::Name);
    if (files.size() < step + 1) {
        std::fprintf(stderr, "need at least %d PNG frames in %s\n", step + 1, qPrintable(args[1]));
        return 2;
    }
    if (!dump_dir.isEmpty())
        QDir().mkpath(dump_dir);

    pl_log log = pl_log_create(PL_API_VER, nullptr);
    struct pl_vk_inst_params inst_params = {};
    pl_vk_inst inst = pl_vk_inst_create(log, &inst_params);
    struct pl_vulkan_params vk_params = {};
    vk_params.instance = inst ? inst->instance : VK_NULL_HANDLE;
    vk_params.get_proc_addr = inst ? inst->get_proc_addr : nullptr;
    vk_params.allow_software = false;
    pl_vulkan vk = inst ? pl_vulkan_create(log, &vk_params) : nullptr;
    if (!vk) {
        std::fprintf(stderr, "Vulkan device creation failed\n");
        return 1;
    }
    pl_gpu gpu = vk->gpu;
    std::printf("GPU: %s\n", vk->phys_device ? "Vulkan device ready" : "?");

    FlowInterpolator flow(log, gpu);
    if (!flow.init()) {
        std::fprintf(stderr, "Flow init failed\n");
        return 1;
    }
    flow.setAnalysisScale(scale);

    pl_fmt fmt = pl_find_named_fmt(gpu, "rgba8");
    auto load = [&](int index) {
        QImage img(dir.filePath(files[index]));
        return img.convertToFormat(QImage::Format_RGBA8888);
    };
    const QImage first = load(0);
    const int w = first.width(), h = first.height();
    auto makeTex = [&](bool storable) {
        struct pl_tex_params p = {};
        p.w = w;
        p.h = h;
        p.format = fmt;
        p.sampleable = true;
        p.storable = storable;
        p.host_writable = true;
        p.host_readable = true;
        return pl_tex_create(gpu, &p);
    };
    pl_tex src[2] = { makeTex(false), makeTex(false) };
    const int gen = step - 1;
    std::vector<pl_tex> outs;
    for (int i = 0; i < gen; i++)
        outs.push_back(makeTex(true));
    if (!src[0] || !src[1] || std::any_of(outs.begin(), outs.end(), [](pl_tex t) { return !t; })) {
        std::fprintf(stderr, "texture creation failed (rgba8 storable?)\n");
        return 1;
    }
    auto upload = [&](pl_tex tex, const QImage &img) {
        struct pl_tex_transfer_params t = {};
        t.tex = tex;
        t.ptr = const_cast<uchar *>(img.constBits());
        t.row_pitch = static_cast<size_t>(img.bytesPerLine());
        return pl_tex_upload(gpu, &t);
    };
    auto download = [&](pl_tex tex) {
        QImage img(w, h, QImage::Format_RGBA8888);
        struct pl_tex_transfer_params t = {};
        t.tex = tex;
        t.ptr = img.bits();
        t.row_pitch = static_cast<size_t>(img.bytesPerLine());
        pl_tex_download(gpu, &t);
        return img;
    };

    std::vector<float> times;
    for (int i = 1; i <= gen; i++)
        times.push_back(static_cast<float>(i) / step);

    Score flow_score, blend_score, repeat_score;
    int cur_slot = 0;
    QImage prev_img = first;
    if (!upload(src[cur_slot], prev_img))
        return 1;
    // Prime the analysis with the first frame.
    flow.interpolate(nullptr, src[cur_slot], outs.data(), times.data(), 0);
    int pairs = 0, dumped = 0;
    QElapsedTimer wall;
    wall.start();
    for (int base = 0; base + step < files.size() && pairs < max_pairs; base += step, pairs++) {
        const QImage next_img = load(base + step);
        const int next_slot = 1 - cur_slot;
        if (!upload(src[next_slot], next_img))
            return 1;
        if (!flow.interpolate(src[cur_slot], src[next_slot], outs.data(), times.data(), gen)) {
            std::fprintf(stderr, "interpolate failed at frame %d\n", base);
            return 1;
        }
        for (int g = 0; g < gen; g++) {
            const QImage truth = load(base + 1 + g);
            const QImage result = download(outs[static_cast<size_t>(g)]);
            double mse, worst;
            compare(result, truth, mse, worst);
            flow_score.add(mse, worst);
            const QImage mixed = blend(prev_img, next_img, times[static_cast<size_t>(g)]);
            compare(mixed, truth, mse, worst);
            blend_score.add(mse, worst);
            compare(times[static_cast<size_t>(g)] < 0.5f ? prev_img : next_img, truth, mse, worst);
            repeat_score.add(mse, worst);
            if (!dump_dir.isEmpty() && dumped < dump_count && (pairs % 10) == 5) {
                const QString stem = QStringLiteral("%1/f%2_t%3").arg(dump_dir).arg(base, 5, 10, QChar('0')).arg(g + 1);
                result.save(stem + "_flow.png");
                truth.save(stem + "_truth.png");
                dumped++;
            }
        }
        prev_img = next_img;
        cur_slot = next_slot;
    }
    const double wall_ms = static_cast<double>(wall.elapsed());
    std::printf("frames %dx%d, %d pictures, step %d (source %d fps from 60), analysis 1/%d\n",
                w, h, pairs, step, 60 / step, scale);
    std::printf("%-22s %10s %16s\n", "method", "PSNR dB", "worst 5% PSNR");
    std::printf("%-22s %10.2f %16.2f\n", "repeat nearest", repeat_score.meanPsnr(), repeat_score.worstPsnr());
    std::printf("%-22s %10.2f %16.2f\n", "blend", blend_score.meanPsnr(), blend_score.worstPsnr());
    std::printf("%-22s %10.2f %16.2f\n", "Flow", flow_score.meanPsnr(), flow_score.worstPsnr());
    std::printf("Flow GPU time: %.2f ms per picture (wall %.0f ms total incl. PNG I/O)\n", flow.gpuTimeMs(), wall_ms);

    for (pl_tex &t : src)
        pl_tex_destroy(gpu, &t);
    for (pl_tex &t : outs)
        pl_tex_destroy(gpu, &t);
    // FlowInterpolator owns GPU objects: destroy it before the device.
    pl_gpu_finish(gpu);
    return 0;
}
