# Paints the 1989 Eastern-bloc paper sack for the cement bag: plain kraft paper, one dark ink,
# simple Cyrillic stencil type (ЦЕМЕНТ, the Soviet portland cement standard GOST 10178-85, 50 kg).
# It replaces Poly Haven's modern red "CEMENT / HIGH STRENGTH / CEM 42.5 R" print, which the source
# stage keeps from the model. The panels are where Poly Haven's UV islands are (2048 px map).
# Run:  powershell -File tools/assets/cement-bag/make-label.ps1      writes base_color.png (1024 px)
# source.mjs copies base_color.png over raw/tex_base_color.png when it sits beside the spec.
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System; using System.Drawing; using System.Drawing.Drawing2D; using System.Drawing.Imaging; using System.Drawing.Text;
public static class CementLabel {
  static Random rnd = new Random(1989);
  static float[,] grid(int n) { var g = new float[n + 1, n + 1]; for (int i = 0; i <= n; i++) for (int j = 0; j <= n; j++) g[i, j] = (float)rnd.NextDouble(); return g; }
  static float sample(float[,] g, int n, float u, float v) { float x = u * n, y = v * n; int i = Math.Min((int)x, n - 1), j = Math.Min((int)y, n - 1); float fx = x - i, fy = y - j;
    float a = g[i, j] * (1 - fx) + g[i + 1, j] * fx, b = g[i, j + 1] * (1 - fx) + g[i + 1, j + 1] * fx; return a * (1 - fy) + b * fy; }
  static Bitmap Label(int w, int h, string[] lines, float[] sizes, string[] fonts, float[] ys, float rule) {
    var bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb);
    using (var g = Graphics.FromImage(bmp)) {
      g.Clear(Color.Transparent); g.TextRenderingHint = TextRenderingHint.AntiAliasGridFit; g.SmoothingMode = SmoothingMode.AntiAlias;
      var sf = new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center, FormatFlags = StringFormatFlags.NoWrap };
      using (var ink = new SolidBrush(Color.FromArgb(255, 28, 32, 46))) {
        for (int i = 0; i < lines.Length; i++) { using (var f = new Font(fonts[i], sizes[i], FontStyle.Regular, GraphicsUnit.Pixel)) g.DrawString(lines[i], f, ink, new RectangleF(0, ys[i] - sizes[i], w, sizes[i] * 2), sf); }
        using (var pen = new Pen(Color.FromArgb(255, 28, 32, 46), rule)) { g.DrawRectangle(pen, rule * 2, rule * 2, w - rule * 4, h - rule * 4); }
      }
    }
    return bmp;
  }
  public static void Make(string outPath) {
    int S = 2048; var paper = new Bitmap(S, S, PixelFormat.Format32bppArgb);
    var low = grid(24); var mid = grid(96);
    for (int y = 0; y < S; y++) for (int x = 0; x < S; x++) {
      float u = x / (float)S, v = y / (float)S;
      float k = 1f + (sample(low, 24, u, v) - 0.5f) * 0.09f + (sample(mid, 96, u, v) - 0.5f) * 0.07f + ((float)rnd.NextDouble() - 0.5f) * 0.035f;
      // long fibres: a stripe noise along x
      k += ((y * 7 % 13) / 13f - 0.5f) * 0.008f;
      int r = (int)Math.Max(0, Math.Min(255, 165 * k)), gg = (int)Math.Max(0, Math.Min(255, 137 * k)), b = (int)Math.Max(0, Math.Min(255, 110 * k));
      paper.SetPixel(x, y, Color.FromArgb(255, r, gg, b));
    }
    // panels in the 2048 map (x0, y0, w, h): front (the face the stand shows), back, side
    DrawPanel(paper, 106, 1124, 1219, 890, new[] { "ЦЕМЕНТ", "ПОРТЛАНДЦЕМЕНТ  М-400", "50 кг", "ГОСТ 10178-85" }, new[] { 215f, 62f, 120f, 50f }, new[] { "Impact", "Impact", "Impact", "Arial Black" }, new[] { 250f, 470f, 650f, 800f });
    DrawPanel(paper, 138, 286, 1092, 827, new[] { "ЦЕМЕНТ", "ПОРТЛАНДЦЕМЕНТ  М-400", "50 кг", "ГОСТ 10178-85" }, new[] { 200f, 60f, 110f, 48f }, new[] { "Impact", "Impact", "Impact", "Arial Black" }, new[] { 230f, 430f, 600f, 740f });
    DrawPanel(paper, 1346, 848, 594, 1123, new[] { "ЦЕМЕНТ  М-400", "50 кг", "ГОСТ 10178-85" }, new[] { 120f, 110f, 46f }, new[] { "Impact", "Impact", "Arial Black" }, new[] { 200f, 360f, 470f });
    var small = new Bitmap(1024, 1024, PixelFormat.Format32bppArgb);
    using (var g = Graphics.FromImage(small)) { g.InterpolationMode = InterpolationMode.HighQualityBicubic; g.DrawImage(paper, 0, 0, 1024, 1024); }
    small.Save(outPath, ImageFormat.Png);
  }
  // text runs bottom-to-top, as Poly Haven's islands are laid out: build it level, turn it a quarter, centre it on the panel
  static void DrawPanel(Bitmap map, int x0, int y0, int w, int h, string[] lines, float[] sizes, string[] fonts, float[] ys) {
    int lw = h - 90, lh = w - 90;
    using (var lab = Label(lw, lh, lines, sizes, fonts, ys, 7)) {
      // print wear: knock the ink out in speckles
      for (int y = 0; y < lh; y++) for (int x = 0; x < lw; x++) { var c = lab.GetPixel(x, y); if (c.A > 0 && rnd.NextDouble() < 0.13) lab.SetPixel(x, y, Color.FromArgb((int)(c.A * (0.25 + 0.5 * rnd.NextDouble())), c.R, c.G, c.B)); }
      lab.RotateFlip(RotateFlipType.Rotate90FlipNone);
      using (var g = Graphics.FromImage(map)) { g.DrawImage(lab, x0 + (w - lab.Width) / 2, y0 + (h - lab.Height) / 2); }
    }
  }
}
'@
[CementLabel]::Make((Join-Path $PSScriptRoot "base_color.png"))
