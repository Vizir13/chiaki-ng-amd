#ifndef CHIAKI_GUI_FGFIXUP_H
#define CHIAKI_GUI_FGFIXUP_H

#include <libplacebo/gpu.h>
#include <libplacebo/log.h>

#include <cstdint>
#include <memory>

// Clean-up passes run on the frames a frame generation backend produced,
// after an idea of VeyAmux (GPL-3, Linux port of Veyra) for FSR on video:
//
//  - Static content (HUD, text, mini-maps, logos): a per-pixel counter of how
//    many real frames in a row a textured pixel stayed unchanged. Static
//    pixels get zero motion before the backend runs (when it takes motion
//    vectors) and the real pixel in the generated frames afterwards; moving
//    background next to them is taken from the frame where it is not covered
//    by the static element, so text is not dragged along ("ghosts").
//  - Detail: interpolation blends both frames moved part of the way. Where
//    the two motion compensated samples disagree (fine or aliased detail,
//    foliage, edges of a moving object), the blend is a smear, and sharp real
//    frames alternating with smeared generated ones reads as flicker while
//    panning. There the generated pixel is replaced by whichever of the two
//    unfiltered samples is closer to it.
//
// Motion is in pixels per pixel of the current frame, "this content was at
// p + mv in the previous frame" (FlowInterpolator::motionVectors).
class FgFixup
{
public:
    FgFixup(pl_log log, pl_gpu gpu);
    ~FgFixup();
    FgFixup(const FgFixup &) = delete;
    FgFixup &operator=(const FgFixup &) = delete;

    bool init();

    // Per real frame, before the backend: updates the static counters from
    // `prev` -> `cur` (prev null: start of a sequence, counters cleared) and,
    // if `motion` is given (storable rg16f), zeroes it on static pixels.
    bool before(pl_tex prev, pl_tex cur, pl_tex motion);

    // Per generated frame at time `t` (0 = prev, 1 = cur): fixes `generated`
    // (storable RGBA8) in place. `motion` may be null (static pixels only).
    bool after(pl_tex prev, pl_tex cur, pl_tex motion, pl_tex generated, float t, bool detail, bool statics);

    void invalidate();

    // GPU time of the passes whose results came in since the last call (ns).
    uint64_t takeGpuNs();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

#endif // CHIAKI_GUI_FGFIXUP_H
