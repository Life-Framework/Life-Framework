#!/bin/sh
# Rewrites the first 8 kits from the finished playtest-town shops (card #2203). Run from the repo root.
# Shops are the pack's store prefabs (offsets are building-local already) plus the clerk the town layer puts
# beside each; bank and clinic are layer groups taken relative to their anchor entity. No police kit: a building agent
# builds that one from scratch (`police-station-kit`), as a whole-building kit in this format (README).
# Stock, outlet ids and alarm kinds are referenced (`config` names the blocks), never copied.
set -e
K="node tools/town/kit.mjs extract"
P=addons/LF_Everon1989/Prefabs/Buildings
VB=addons/LF_Everon1989/Worlds/Everon1989Town/Everon1989Town_Layers/Village_Buildings.layer
DBG=addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers
GRID=addons/LifeFramework/Worlds/GridWorld/GridWorld_Layers

# general store (#1884): the building prefab, the register, radio, clerk and alarm the debug-world layer places beside it
$K $P/GeneralStore/GeneralStore.et '*' --kit general-store --system store \
  --building $VB::Village_GeneralStore \
  --with "$DBG/Shops.layer::Shop_GeneralStore_Till,Shop_GeneralStore_Radio,Shop_Clerk,Shop_GeneralStore_AlarmBox"

# supermarket
$K $P/Supermarket/Supermarket_Store.et '*' --kit supermarket --system store --drop probe-house-base \
  --building $VB::Village_Supermarket --with "$VB::Village_Supermarket_*"

# drug store (#1898), gun store (#1903, #2013), clothing store (#2009)
$K $P/Drugstore/Drugstore_Store.et '*' --kit drugstore --system store \
  --building $VB::Village_Drugstore --with "$VB::Village_Drugstore_*"
$K $P/GunStore/GunStore_Store.et '*' --kit gun-store --system gun-counter \
  --building $VB::Village_GunStore --with "$VB::Village_GunStore_*"
$K $P/ClothingStore/ClothingStore_Store.et '*' --kit clothing-store --system clothing \
  --building $VB::Village_ClothingStore --with "$VB::Village_ClothingStore_*"

# bank counter and vault: the vault (town layer) and the alarm, bag and rob kit (debug-world layer), relative to the vault
$K "$VB,$DBG/Street_Bank.layer" 'Roadmap_Bank_*' --kit bank-vault --system bank --origin Roadmap_Bank_Vault

# clinic: duty desk, doctor, dispatch board, bench and payphone, relative to the duty desk
$K $DBG/Street_Hospital.layer 'Roadmap_Hospital_*,Hospital_*' --kit clinic --system medic \
  --origin Roadmap_Hospital_Duty_Desk --exclude 'Roadmap_Hospital_Ambulance,Roadmap_Hospital_Patient,Hospital_Bin'

# fence stall (grid cell fence-shop-2), relative to the stall
$K $GRID/Grid_fence_shop_2.layer 'FenceStall_*,Grid_fence_shop_2_Sign' --kit fence-stall --system fence --origin FenceStall_Grid
