// tools/livery/panels.mjs - where each body panel sits in a vehicle's mask (UV set 2), in pixels of the
// 512 px mask: { x, y, w, h } top-left corner and size, plus `rot` / `flipx` when the panel's UVs are
// turned. Lettering in `mode: "panels"` lands centred on the panel it names.
//
// Not measured yet: the rectangles have to come from UV set 2 of each part's .xob (the LZ4-packed LODS
// chunk, whose vertex layout tools/lib/xob.mjs does not decode yet; split from #4232 into #4285). Probing with
// labelled mask cells took three boots and stayed unreadable (docs/architecture/livery-spike.md), so a
// panel is only added here from a real reading. Until then use `mode: "tiled"`.
export const MASK_SIZE = 512;

export const PANELS = {
  UAZ469: {
    // door_L01: { x, y, w, h },
  },
};
