# xBRZ 1.9 — unmodified upstream source

Author: Zenju (`zenju AT gmx DOT de`). Project: <https://sourceforge.net/projects/xbrz/>.

The six upstream files in this directory were extracted byte for byte from the
official `xBRZ_1.9.zip` release, dated 2026-01-24 (uploaded 2026-01-25):

<https://sourceforge.net/projects/xbrz/files/xBRZ/xBRZ_1.9.zip/download>

The archive SHA-256 is
`b2dff73b3abd24a18a7cde78d5ff5ed8f0922296dce6ed734dce2264cd0a0fc9`,
matching the checksum published by SourceForge. `UPSTREAM.json` records each
extracted file hash. `README.ra2toolbox.md` and `UPSTREAM.json` are local
provenance records; the upstream algorithm, defaults and source are unchanged.

## License

The original copyright and GPL v3 notices, including Zenju's linking exception
for the specifically listed applications, remain in each source header.
`License.txt` is the original GPL v3 text from the archive. RA2 Toolbox uses
xBRZ under GPL v3; the named-application exception is not needed for this project.

## Integration constraints

- Compile `xbrz.cpp` once per executable that needs it. Upstream 1.9 uses one
  C++23 lambda syntax extension (`[&] -> bool`); GCC and Zig 0.13 Clang accept it
  in the project's C++17 mode with an extension warning. The source is kept
  unchanged; C++23 mode also avoids that warning.
- `xbrz::scale` accepts integer factors 2 through 6. Toolbox can choose a factor
  from its actual output ratio and resolve the intermediate image to the exact
  target size. This does not imply that the final display must be exactly 2x.
- `ColorFormat::argb` and `argbUnbuffered` use **straight** `0xAARRGGBB` input
  and output. Toolbox's compositor uses premultiplied ARGB, so the adapter must
  unpremultiply before xBRZ and premultiply its output. Fully transparent input
  should be canonicalized to zero.
- `argbUnbuffered` avoids the buffered formats' process-wide 64 MiB color-distance
  lookup table and its one-time initialization. The tradeoff is slower distance
  evaluation; retain the upstream default `ScalerCfg` for algorithm fidelity.
- The public `xbrz::bilinearScale` also uses straight ARGB and applies
  alpha-weighted interpolation internally. Its template in `xbrz_tools.h` treats
  alpha as a normal channel; that template requires caller-managed alpha handling.
- The library sees colors, not palette indices, depth, shroud, live lighting or
  emissive flags. Toolbox must supply current rendered colors and maintain its
  own coverage, depth, visibility and cache invalidation rules. Any cache of lit
  pixels must be invalidated when their lighting or source palette changes.

The checksums establish source identity, not real-game visual quality or speed.
