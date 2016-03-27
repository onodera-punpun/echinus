## Echinus

Hmm, I still gotta write this entire README, anyways, the only differences
between upstream and my fork (for now) are:

* Added a fibonacci tiling layout.
* Removed the original tiling layout.
* Added gaps.
* Added margins.
* added a `togglefloatingtag` options, it is similar to `togglefloating`,
  however it toggles tiling/floating for every window in the tag.
* Renamed `togglefloating` to `togglefloatingwin`.
* Added a `deftilinglayout` option, used by `togglefloatingtag`.
* Renamed to `command` option to `rootrightclick`.
* Made it possible to set exectue a command when scrolling over the root
  window with `rootscroll{up,down}`.
* Customized the Makefile a bit, still very much WIP.
