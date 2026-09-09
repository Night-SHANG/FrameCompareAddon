# Behavioral references

FrameCompare was designed after studying established split-screen/Before-After behavior instead of inventing the presentation model from scratch.

The user supplied two legacy ReShade shaders during design:

- `SplitScreenCR.fx` — original comment credits Ganossa and a ReShade 4.x port by CRubino. It backs up the backbuffer, has a configurable border, a ping-pong slider speed and characteristic center-offset UV sampling.
- `Splitscreen.fx` — CeeJay.dk version 2.0, MIT licensed, with conventional Before/After split modes.

The runtime implementation in this repository is independent C++/ReShade FX code. The user-supplied `SplitScreenCR.fx` and `sMask.png` are intentionally **not redistributed** because their redistribution license was not established. FrameCompare does not need those files at runtime.

The `SplitScreenCR-style` mode in `FrameCompare.fx` reproduces the useful visual behavior (center-remapped sampling around a movable divider) with newly written code rather than copying the legacy source.
