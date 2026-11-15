# Source history and contribution branches

The Icarus checkout came from DoubleDensity/fbterm, branch `nerd_fonts`,
commit c26aa2c. That GitHub fork was transferred to Section-9-Workflows/fbterm.
The parent/source repository is https://github.com/sfzhi/fbterm, whose initial
commit imported FbTerm 1.7 from Google Code. The common upstream base is
419c6be55413a7069fbcc66883d58caa94ba3933. Original history is retained unchanged.

Prepared review branches (no upstream PRs opened):

| Branch | Review base | Contents |
| --- | --- | --- |
| review/unicode-emoji | upstream/master | 32-bit Unicode cells, glyph and color-emoji rendering |
| review/scrolling | review/unicode-emoji | Scroll preservation and framebuffer update changes |
| review/screen-blanking | review/scrolling | Idle blanking and redraw on wake |
| review/autotools-refresh | upstream/master | Original generated Autotools changes, kept out of product reviews |

The first three form a stack, not three independent PRs against upstream/master.
The initial combined src tree, excluding generated Makefile.in files, matched
the original nerd_fonts tip byte for byte. Later commits clarify comments,
including the generated configuration comment for idle blanking. They preserve
the historical implementation and its existing quirks. Each intermediate branch
still needs a correctness review and compilation before an upstream proposal.
Maintainers should probably regenerate the Autotools files for their toolchain.

`poulsback-native` starts from the complete working nerd_fonts tip. Native renderer
and Debian/Depot packaging are separate commits and must stay out of the old
feature proposals. New commits use DoubleDensity <doubledense@gmail.com>.
