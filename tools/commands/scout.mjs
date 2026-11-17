// tools\cli scout: a location dossier for a named place on vanilla Everon (tools/scout/scout.mjs).
import { main as scout } from "../scout/scout.mjs";

export const help = "scout <place | x,z> [--radius m] [--quick] | list [filter] | types   location dossier of vanilla Everon: buildings, footprints, roads, free plots, pictures -> docs/everon/places/<slug>/";

export async function main(args, { root }) {
  return scout(args, { root });
}
