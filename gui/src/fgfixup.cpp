#include "fgfixup.h"

#include <libplacebo/dispatch.h>
#include <libplacebo/shaders/custom.h>

#include <QLoggingCategory>

#include <initializer_list>
#include <string>
#include <vector>

Q_DECLARE_LOGGING_CATEGORY(chiakiGui);

namespace {

// Real frames a textured pixel must stay unchanged to count as static.
constexpr float kStaticFrames = 6.0f;
// Max channel difference (0..1) between the two motion compensated samples
// above which the generated pixel is treated as a smear.
constexpr float kDetailThreshold = 0.08f;

const char *kCommonHeader = R"GLSL(
float fx_diff(vec3 a, vec3 b)
{
    vec3 d = abs(a - b);
    return max(max(d.r, d.g), d.b);
}
)GLSL";

// Static counter update (and zero motion on static pixels).
const char *kBeforeBody = R"GLSL(
ivec2 p = ivec2(gl_GlobalInvocationID.xy);
ivec2 lim = ivec2(fxSize) - 1;
if (p.x > lim.x || p.y > lim.y)
    return;
vec3 d = abs(texelFetch(fxCur, p, 0).rgb - texelFetch(fxPrev, p, 0).rgb);
float n = max(max(d.r, d.g), d.b) <= 2.5 / 255.0 ? min(imageLoad(fxStable, p).r + 1.0, 250.0) : 0.0;
// Only textured pixels can be static UI: a flat area of a panning picture
// also stays unchanged pixel by pixel but interpolates correctly anyway.
float lo = 1.0;
float hi = 0.0;
for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
        float l = dot(texelFetch(fxCur, clamp(p + ivec2(i, j), ivec2(0), lim), 0).rgb, vec3(0.299, 0.587, 0.114));
        lo = min(lo, l);
        hi = max(hi, l);
    }
}
if (hi - lo < 0.12)
    n = min(n, fxNeed - 1.0);
imageStore(fxStable, p, vec4(n, 0.0, 0.0, 0.0));
#ifdef FX_HAS_MV
if (n >= fxNeed)
    imageStore(fxMotion, p, vec4(0.0));
#endif
)GLSL";

const char *kClearBody = R"GLSL(
ivec2 p = ivec2(gl_GlobalInvocationID.xy);
if (p.x >= int(fxSize.x) || p.y >= int(fxSize.y))
    return;
imageStore(fxStable, p, vec4(0.0));
)GLSL";

const char *kAfterHeader = R"GLSL(
float fx_diff(vec3 a, vec3 b)
{
    vec3 d = abs(a - b);
    return max(max(d.r, d.g), d.b);
}

bool fx_inside(vec2 q)
{
    return all(greaterThanEqual(q, vec2(0.0))) && all(lessThanEqual(q, fxSize - 1.0));
}

vec3 fx_fetch_cur(vec2 q)
{
    return texelFetch(fxCur, clamp(ivec2(floor(q + 0.5)), ivec2(0), ivec2(fxSize) - 1), 0).rgb;
}

vec3 fx_fetch_prev(vec2 q)
{
    return texelFetch(fxPrev, clamp(ivec2(floor(q + 0.5)), ivec2(0), ivec2(fxSize) - 1), 0).rgb;
}

bool fx_static(ivec2 c)
{
    return texelFetch(fxStable, clamp(c, ivec2(0), ivec2(fxSize) - 1), 0).r >= fxNeed;
}

// Static within one pixel of q (covers the anti-aliased rim of text).
bool fx_static_near(vec2 q)
{
    ivec2 c = ivec2(floor(q + 0.5));
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++)
            if (fx_static(c + ivec2(i, j)))
                return true;
    return false;
}
)GLSL";

// Per generated pixel p at time t: the content was at p - (1 - t) * mv in
// the current frame and at p + t * mv in the previous one.
const char *kAfterBody = R"GLSL(
ivec2 p = ivec2(gl_GlobalInvocationID.xy);
if (p.x >= int(fxSize.x) || p.y >= int(fxSize.y))
    return;
vec4 g = imageLoad(fxGen, p);
#ifdef FX_HAS_MV
vec2 mv = texelFetch(fxMotion, p, 0).xy;
#else
vec2 mv = vec2(0.0);
#endif
vec2 pc = vec2(p);
vec2 qc = pc - (1.0 - fxT) * mv;
vec2 qp = pc + fxT * mv;
vec2 inv = 1.0 / fxSize;

if (fxDetail > 0 && fx_inside(qc) && fx_inside(qp)) {
    // Decide on filtered samples: where the motion fits they agree even on
    // sharp edges and the backend's result stays; only a real mismatch
    // switches to a single unfiltered sample.
    vec3 fa = textureLod(fxCur, (qc + 0.5) * inv, 0.0).rgb;
    vec3 fb = textureLod(fxPrev, (qp + 0.5) * inv, 0.0).rgb;
    if (fx_diff(fa, fb) > fxThreshold) {
        vec3 a = fx_fetch_cur(qc);
        vec3 b = fx_fetch_prev(qp);
        g.rgb = fx_diff(a, g.rgb) <= fx_diff(b, g.rgb) ? a : b;
    }
}

if (fxStatics > 0) {
    float s = 0.0;
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++)
            s += fx_static(p + ivec2(i, j)) ? 1.0 : 0.0;
    float w = fx_static(p) ? clamp(s / 6.0, 0.0, 1.0) : 0.0;
    if (w > 0.0) {
        // Identical in both real frames: the real pixel.
        g.rgb = mix(g.rgb, texelFetch(fxCur, p, 0).rgb, w);
    } else if (dot(mv, mv) >= 0.25) {
        bool sc = fx_static_near(qc);
        bool sp = fx_static_near(qp);
        if (sc && sp)
            g.rgb = texelFetch(fxCur, p, 0).rgb;
        else if (sc)
            g.rgb = textureLod(fxPrev, (qp + 0.5) * inv, 0.0).rgb;
        else if (sp)
            g.rgb = textureLod(fxCur, (qc + 0.5) * inv, 0.0).rgb;
    }
}
imageStore(fxGen, p, g);
)GLSL";

struct pl_shader_desc texDesc(const char *name, pl_tex tex, bool linear)
{
    struct pl_shader_desc d = {};
    d.desc.name = name;
    d.desc.type = PL_DESC_SAMPLED_TEX;
    d.binding.object = tex;
    d.binding.sample_mode = linear ? PL_TEX_SAMPLE_LINEAR : PL_TEX_SAMPLE_NEAREST;
    d.binding.address_mode = PL_TEX_ADDRESS_CLAMP;
    return d;
}

struct pl_shader_desc imgDesc(const char *name, pl_tex tex)
{
    struct pl_shader_desc d = {};
    d.desc.name = name;
    d.desc.type = PL_DESC_STORAGE_IMG;
    d.desc.access = PL_DESC_ACCESS_READWRITE;
    d.binding.object = tex;
    return d;
}

struct pl_shader_var floatVar(const char *name, const float *data)
{
    struct pl_shader_var v = {};
    v.var = pl_var_float(name);
    v.data = data;
    return v;
}

struct pl_shader_var vec2Var(const char *name, const float *data)
{
    struct pl_shader_var v = {};
    v.var = pl_var_vec2(name);
    v.data = data;
    return v;
}

struct pl_shader_var intVar(const char *name, const int *data)
{
    struct pl_shader_var v = {};
    v.var = pl_var_int(name);
    v.data = data;
    return v;
}

} // namespace

struct FgFixup::Impl
{
    pl_log log = nullptr;
    pl_gpu gpu = nullptr;
    pl_dispatch dp = nullptr;
    pl_fmt fmt_stable = nullptr;
    pl_tex stable = nullptr;
    int width = 0;
    int height = 0;
    bool warned = false;
    // Ring of timers for the passes; results are read in takeGpuNs().
    std::vector<pl_timer> timers;
    size_t timer_cursor = 0;

    bool ensureSize(int w, int h);
    bool run(const char *description, const char *header, const char *body,
             std::initializer_list<struct pl_shader_desc> descs,
             std::initializer_list<struct pl_shader_var> vars, int w, int h);
};

bool FgFixup::Impl::ensureSize(int w, int h)
{
    if (stable && w == width && h == height)
        return true;
    pl_tex_destroy(gpu, &stable);
    struct pl_tex_params params = {};
    params.w = w;
    params.h = h;
    params.format = fmt_stable;
    params.sampleable = true;
    params.storable = true;
    stable = pl_tex_create(gpu, &params);
    if (!stable)
        return false;
    width = w;
    height = h;
    const float size[2] = { static_cast<float>(w), static_cast<float>(h) };
    return run("fg fixup clear", "", kClearBody, { imgDesc("fxStable", stable) }, { vec2Var("fxSize", size) }, w, h);
}

bool FgFixup::Impl::run(const char *description, const char *header, const char *body,
                        std::initializer_list<struct pl_shader_desc> descs,
                        std::initializer_list<struct pl_shader_var> vars, int w, int h)
{
    constexpr int kGroup = 16;
    pl_shader sh = pl_dispatch_begin(dp);
    struct pl_custom_shader params = {};
    params.description = description;
    params.header = header;
    params.body = body;
    params.input = PL_SHADER_SIG_NONE;
    params.output = PL_SHADER_SIG_NONE;
    params.descriptors = descs.begin();
    params.num_descriptors = static_cast<int>(descs.size());
    params.variables = vars.begin();
    params.num_variables = static_cast<int>(vars.size());
    params.compute = true;
    params.compute_group_size[0] = kGroup;
    params.compute_group_size[1] = kGroup;
    if (!pl_shader_custom(sh, &params)) {
        pl_dispatch_abort(dp, &sh);
        if (!warned)
            qCWarning(chiakiGui) << "[framegen] Clean-up shader setup failed:" << description;
        warned = true;
        return false;
    }
    struct pl_dispatch_compute_params compute = {};
    compute.shader = &sh;
    compute.dispatch_size[0] = (w + kGroup - 1) / kGroup;
    compute.dispatch_size[1] = (h + kGroup - 1) / kGroup;
    compute.dispatch_size[2] = 1;
    if (!timers.empty()) {
        compute.timer = timers[timer_cursor];
        timer_cursor = (timer_cursor + 1) % timers.size();
    }
    if (!pl_dispatch_compute(dp, &compute)) {
        if (!warned)
            qCWarning(chiakiGui) << "[framegen] Clean-up shader dispatch failed:" << description;
        warned = true;
        return false;
    }
    return true;
}

FgFixup::FgFixup(pl_log log, pl_gpu gpu)
    : impl(std::make_unique<Impl>())
{
    impl->log = log;
    impl->gpu = gpu;
}

FgFixup::~FgFixup()
{
    Impl &d = *impl;
    if (!d.gpu)
        return;
    pl_tex_destroy(d.gpu, &d.stable);
    for (pl_timer &timer : d.timers)
        pl_timer_destroy(d.gpu, &timer);
    pl_dispatch_destroy(&d.dp);
}

bool FgFixup::init()
{
    Impl &d = *impl;
    if (d.dp)
        return true;
    if (!d.gpu || !d.gpu->glsl.compute)
        return false;
    const auto needed = static_cast<enum pl_fmt_caps>(PL_FMT_CAP_SAMPLEABLE | PL_FMT_CAP_STORABLE);
    for (const char *name : { "r16f", "r32f", "rgba16f" }) {
        pl_fmt fmt = pl_find_named_fmt(d.gpu, name);
        if (fmt && (fmt->caps & needed) == needed && fmt->glsl_format) {
            d.fmt_stable = fmt;
            break;
        }
    }
    if (!d.fmt_stable) {
        qCWarning(chiakiGui) << "[framegen] Generated frame clean-up needs a storable float texture format";
        return false;
    }
    d.dp = pl_dispatch_create(d.log, d.gpu);
    if (d.dp) {
        d.timers.assign(16, nullptr);
        for (pl_timer &timer : d.timers)
            timer = pl_timer_create(d.gpu);
    }
    return d.dp != nullptr;
}

uint64_t FgFixup::takeGpuNs()
{
    uint64_t total = 0;
    for (pl_timer timer : impl->timers) {
        if (!timer)
            continue;
        uint64_t ns;
        while ((ns = pl_timer_query(impl->gpu, timer)) != 0)
            total += ns;
    }
    return total;
}

void FgFixup::invalidate()
{
    Impl &d = *impl;
    pl_tex_destroy(d.gpu, &d.stable);
    d.width = d.height = 0;
}

bool FgFixup::before(pl_tex prev, pl_tex cur, pl_tex motion)
{
    Impl &d = *impl;
    if (!d.dp || !cur)
        return false;
    const int w = cur->params.w;
    const int h = cur->params.h;
    const bool resized = !d.stable || w != d.width || h != d.height;
    if (!d.ensureSize(w, h))
        return false;
    const float size[2] = { static_cast<float>(w), static_cast<float>(h) };
    if (!prev || prev == cur || prev->params.w != w || prev->params.h != h) {
        if (resized)
            return true;
        return d.run("fg fixup clear", "", kClearBody, { imgDesc("fxStable", d.stable) }, { vec2Var("fxSize", size) }, w, h);
    }
    const float need = kStaticFrames;
    if (motion) {
        static const std::string header = std::string("#define FX_HAS_MV 1\n") + kCommonHeader;
        return d.run("fg fixup static", header.c_str(), kBeforeBody,
                     { texDesc("fxPrev", prev, false), texDesc("fxCur", cur, false), imgDesc("fxStable", d.stable),
                       imgDesc("fxMotion", motion) },
                     { vec2Var("fxSize", size), floatVar("fxNeed", &need) }, w, h);
    }
    return d.run("fg fixup static", kCommonHeader, kBeforeBody,
                 { texDesc("fxPrev", prev, false), texDesc("fxCur", cur, false), imgDesc("fxStable", d.stable) },
                 { vec2Var("fxSize", size), floatVar("fxNeed", &need) }, w, h);
}

bool FgFixup::after(pl_tex prev, pl_tex cur, pl_tex motion, pl_tex generated, float t, bool detail, bool statics)
{
    Impl &d = *impl;
    if (!d.dp || !prev || !cur || !generated || !d.stable)
        return false;
    const int w = cur->params.w;
    const int h = cur->params.h;
    if (w != d.width || h != d.height || generated->params.w != w || generated->params.h != h)
        return false;
    detail = detail && motion;
    if (!detail && !statics)
        return true;
    const float size[2] = { static_cast<float>(w), static_cast<float>(h) };
    const float need = kStaticFrames;
    const float threshold = kDetailThreshold;
    const int detail_on = detail ? 1 : 0;
    const int statics_on = statics ? 1 : 0;
    if (motion) {
        static const std::string header = std::string("#define FX_HAS_MV 1\n") + kAfterHeader;
        return d.run("fg fixup", header.c_str(), kAfterBody,
                     { texDesc("fxPrev", prev, true), texDesc("fxCur", cur, true), texDesc("fxMotion", motion, false),
                       texDesc("fxStable", d.stable, false), imgDesc("fxGen", generated) },
                     { vec2Var("fxSize", size), floatVar("fxT", &t), floatVar("fxThreshold", &threshold),
                       floatVar("fxNeed", &need), intVar("fxDetail", &detail_on), intVar("fxStatics", &statics_on) },
                     w, h);
    }
    return d.run("fg fixup", kAfterHeader, kAfterBody,
                 { texDesc("fxPrev", prev, true), texDesc("fxCur", cur, true), texDesc("fxStable", d.stable, false),
                   imgDesc("fxGen", generated) },
                 { vec2Var("fxSize", size), floatVar("fxT", &t), floatVar("fxThreshold", &threshold),
                   floatVar("fxNeed", &need), intVar("fxDetail", &detail_on), intVar("fxStatics", &statics_on) },
                 w, h);
}
