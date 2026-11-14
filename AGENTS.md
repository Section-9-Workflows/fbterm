# FbTerm fork

Preserve the owner's nerd_fonts branch and its font, emoji, scroll and blanking
customizations. Upstream is sfzhi/fbterm; origin is Section-9-Workflows/fbterm.
Use DoubleDensity <doubledense@gmail.com> for new commits. Keep build integration
and renderer changes in separate commits; do not rewrite existing history.

No unit tests, synthetic probes, mocks or generated test data. Validate through
compilation and actual application/owner workflows. Use Depot for compilation;
keep artifacts in Depot Registry, not GitHub artifact storage. Use Nix for host
tooling and container packages for build dependencies. Do not change Icarus's
system packages or replace its working login terminal for development.

Poulsback uses its native, pinned UAPI, inherited mode and typed scanout BOs.
No raw registers, reset/unbind/unload recovery or CPU fallback after an unknown
GPU result. Runtime acceleration and VT handoff require real hardware validation.
