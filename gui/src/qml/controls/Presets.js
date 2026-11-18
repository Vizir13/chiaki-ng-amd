.pragma library

// Smoothness presets. A preset is only a set of values for existing
// settings (Chiaki.settings.*); nothing new is stored. The active preset is
// whichever one the current settings match, otherwise "Custom".

function vsrMode(s) {
    if (!s.rtxVsrSupported)
        return 0;
    // Streaming Ultra (upscale + artifact cleanup) when the runtime has it
    return s.rtxVsrExtendedModes ? 23 : 4;
}

function vfgReady(s) {
    return s.frameGenVfgSupported && s.frameGenVfgRuntime.length > 0;
}

function generator(s) {
    if (vfgReady(s))
        return 4;
    if (s.frameGenFlowSupported)
        return 3;
    return 0;
}

function list(s) {
    const gen = generator(s);
    const smooth = { rtxVsrQuality: vsrMode(s), frameGenBackend: gen, frameGenMultiplier: 2, frameGenVfgChain: false, nvidiaReflex: true };
    if (gen === 4)
        smooth.frameGenVfgMode = 2;
    const max = { rtxVsrQuality: vsrMode(s), frameGenBackend: gen, nvidiaReflex: true };
    if (gen === 4 && s.frameGenDlssSupported) {
        max.frameGenVfgChain = true;
        max.frameGenMultiplier = 2;
        max.frameGenVfgMode = 2;
    } else {
        max.frameGenVfgChain = false;
        max.frameGenMultiplier = 4;
        if (gen === 4)
            max.frameGenVfgMode = 2;
    }
    const maxDesc = max.frameGenVfgChain
        ? qsTr("VFG 2× + DLSS FG 2×. Needs a 120 Hz+ display.")
        : qsTr("Frame generation 4×. Needs a 120 Hz+ display.");
    return [
        { id: "native", name: qsTr("Native"), desc: qsTr("Stream as it comes. Lowest latency."),
          values: { rtxVsrQuality: 0, frameGenBackend: 0, frameGenVfgChain: false }, available: true },
        { id: "sharp", name: qsTr("Sharp"), desc: qsTr("RTX Video upscaling, no frame generation."),
          values: { rtxVsrQuality: vsrMode(s), frameGenBackend: 0, frameGenVfgChain: false }, available: s.rtxVsrSupported },
        { id: "smooth", name: qsTr("Smooth 30 → 60"), desc: gen === 4 ? qsTr("Upscaling + VFG 2× + Reflex. Best for 30 fps games.") : qsTr("Upscaling + frame generation 2× + Reflex. Best for 30 fps games."),
          values: smooth, available: s.frameGenSupported && gen > 0 },
        { id: "max", name: qsTr("Max 4×"), desc: maxDesc,
          values: max, available: s.frameGenSupported && gen > 0 }
    ];
}

// Any RTX Video mode counts as "upscaling on": the user's own choice of
// mode (Upscale Ultra, Streaming, ...) is kept.
function matches(s, values) {
    for (const key in values) {
        if (key === "rtxVsrQuality") {
            if ((s.rtxVsrQuality > 0) !== (values.rtxVsrQuality > 0))
                return false;
        } else if (s[key] !== values[key]) {
            return false;
        }
    }
    return true;
}

// Returns the id of the active preset, or "custom"
function current(s) {
    const all = list(s);
    for (let i = 0; i < all.length; ++i) {
        if (all[i].available && matches(s, all[i].values))
            return all[i].id;
    }
    return "custom";
}

function find(s, id) {
    const all = list(s);
    for (let i = 0; i < all.length; ++i) {
        if (all[i].id === id)
            return all[i];
    }
    return null;
}

function currentName(s) {
    const id = current(s);
    if (id === "custom")
        return qsTr("Custom");
    return find(s, id).name;
}

// Every setting a preset touches: "Custom" is a snapshot of these.
const keys = ["rtxVsrQuality", "frameGenBackend", "frameGenMultiplier", "frameGenVfgChain", "frameGenVfgMode", "nvidiaReflex"];

function snapshot(s) {
    let v = {};
    for (const k of keys)
        v[k] = s[k];
    return v;
}

// Frame generation only runs with Direct Mapping, which needs a restart.
function needsRestart(s, values) {
    return values.frameGenBackend > 0 && !s.directFrameMapping;
}

function apply(s, values, exact) {
    for (const key in values) {
        // Keep the user's own RTX Video mode when a preset only wants it on
        if (!exact && key === "rtxVsrQuality" && values[key] > 0 && s.rtxVsrQuality > 0)
            continue;
        s[key] = values[key];
    }
}

// Frames shown per stream frame with the current settings
function outputFactor(s) {
    if (!s.frameGenSupported || s.frameGenBackend === 0 || !s.directFrameMapping)
        return 1;
    if (s.frameGenBackend === 2)
        return 2;
    if (s.frameGenBackend === 4 && s.frameGenVfgChain && s.frameGenDlssSupported)
        return 4;
    return s.frameGenMultiplier;
}

function resolutionName(index) {
    switch (index) {
    case 1: return "360p";
    case 2: return "540p";
    case 3: return "720p";
    case 4: return "1080p";
    default: return "?";
    }
}

// "1080p · 30 fps" for a host (local or remote, PS4 or PS5)
function streamSummary(s, host) {
    const local = !host || host.discovered || !host.duid;
    const ps5 = !host || host.ps5;
    let res, fps;
    if (ps5) {
        res = local ? s.resolutionLocalPS5 : s.resolutionRemotePS5;
        fps = local ? s.fpsLocalPS5 : s.fpsRemotePS5;
    } else {
        res = local ? s.resolutionLocalPS4 : s.resolutionRemotePS4;
        fps = local ? s.fpsLocalPS4 : s.fpsRemotePS4;
    }
    return { text: qsTr("%1 · %2 fps").arg(resolutionName(res)).arg(fps), fps: fps };
}

function screenSummary(s, streamFps) {
    const res = s.processingHeight > 0 ? qsTr("%1p").arg(s.processingHeight) : qsTr("Fit to screen");
    return qsTr("%1 · %2 fps").arg(res).arg(streamFps * outputFactor(s));
}
