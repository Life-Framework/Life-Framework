# concepts-local: free concept pictures on this PC

The optional provider for the concept stage (`tools\cli asset run <slug> --only concept --provider local`). The default
provider is Meshy (`docs/asset-pipeline/concepts.md`). Nothing here is in the repo except the code: the runtime and the
weights live in `%LOCALAPPDATA%\LifeFramework` (`LF_LOCAL_HOME` overrides).

| Part | What | Licence |
|---|---|---|
| Runtime | [stable-diffusion.cpp](https://github.com/leejet/stable-diffusion.cpp) `master-1094-3bad892`, CUDA 12 build, `sd-cli.exe` | MIT |
| Model | FLUX.1 [schnell], city96 GGUF `Q4_K_S` (6.8 GB) | Apache-2.0 |
| Text encoders | T5 v1.1 XXL encoder `Q5_K_M` (3.4 GB, city96), CLIP-L (comfyanonymous) | Apache-2.0 |
| VAE | `ae.safetensors`, sha256 `afc8e282...9e38` (the FLUX.1 autoencoder; the official repo is login-gated, this is a public identical copy) | Apache-2.0 |

Licence evidence: the Hugging Face metadata of `black-forest-labs/FLUX.1-schnell` and of the city96 and comfyanonymous
repos all say `apache-2.0` (read 2026-11-14). The BFL repo has no separate LICENSE file; the model card is the grant.
`docs/credits.md` and `docs/decisions.md` record it. FLUX.1 [dev] is non-commercial and is not used.

## Commands

```
node tools/concepts-local/local.mjs fetch     runtime + weights, resumable, sha256-checked (about 11 GB)
node tools/concepts-local/local.mjs status    what is installed, the GPU reading
node tools/concepts-local/local.mjs gen -p "..." -o out.png [-i ref.png --strength 0.7] [--size 1024] [-s seed] [--wait 120]
```

`-i` is image-to-image: a `reference` picture in `concepts.json` becomes the start image (`reference.strength`, default 0.7).
Four steps, euler, cfg 1, weights offloaded to RAM (`--offload-to-cpu`), tiled VAE. `sd-cli` is launched with
`windowsHide`, so no window opens.

## Never crash the maintainer's game

- Before it starts it reads `nvidia-smi`; with less than 7600 MiB free (`LF_LOCAL_NEED_MIB`) it refuses with exit code 3 and
  the reason (a game session or Workbench holds the GPU). `--wait <s>` polls for up to that long instead.
- While it runs, free VRAM under 200 MiB kills it. A 15 minute timeout kills it. A lock file stops two runs overlapping.
- Needs about 11 GB of disk, and RAM for the offloaded weights (Q4_K_S plus T5 Q5 is about 10 GB mapped; close sessions on a
  15.6 GB machine).

## Status

Installed and checksummed on this PC on 2026-11-14. The first timed run (seconds per image, VRAM peak) did not happen: every
attempt found a game session or Workbench holding the GPU (4.7 GB free of 10.2) and the guard refused, as designed. The numbers
belong here once a run completes on an idle GPU: `node tools/concepts-local/local.mjs gen -p "a pager" -o tmp/x.png`.
