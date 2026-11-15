# Native Poulsback backend

Build with `--enable-poulsback`, then select `fbterm --poulsback` on the active
Linux VT. If a login service starts it on an inactive VT, it waits for that VT
to become active before opening the display device. Default fbdev behavior
stays available without that option. No X or
Wayland is involved. The backend checks the Poulsback DRM driver, inherited
single LVDS mode and 2D-compatible pitch; it does not resize or change timings.

This first implementation creates a typed scanout BO, uses native SUBMIT_2D
for overlapping scroll copies, and waits on the buffer's native dma-buf fence
before any following CPU glyph writes. Nerd Font and color-emoji rasterization
stay in the existing renderer. CPU writes remain appropriate for glyphs and
simple fills; scrolling avoids reading the slow write-combined framebuffer.
Actual speed gains have not yet been measured.

VT release restores the saved legacy framebuffer before acknowledging release.
A failed handoff refuses the VT switch. Panel blanking uses the selected
connector's DPMS property. Rotation and framebuffer background images are
currently refused explicitly by this backend; the original fbdev path supports
them. DRM descriptors are close-on-exec; child shells do not inherit them.

Errors stop native drawing without GPU reset/unload or silently retrying on the
CPU. File close follows the driver's own fence/HOLD retention contract. The
current driver initializes the engine at first context creation and owns it for
the rest of the boot, blocking PM after that. Suspend is not qualified. Native
fence waiting orders CPU access but does not by itself report a job fence's
error status; kernel diagnostics must be reviewed during hardware qualification.

## Validation boundary

Compilation does not qualify the GPU or VT path. First use must be a real
terminal session on the owner machine, with SSH available for inspection. Check
normal text/emoji, scrolling, exit/VT restoration and blank/wake. Inspect the
native driver job/error counters and kernel log. On an unknown GPU result,
retain state and collect diagnostics; do not retry/reset/unload. Reboot through
the preserved stock-driver or Gold Master entry for recovery.
