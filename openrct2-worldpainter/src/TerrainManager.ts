/*****************************************************************************
 * Copyright (c) 2024-2026 Sadret
 *
 * The OpenRCT2 plug-in "WorldPainter" is licensed
 * under the GNU General Public License version 3.
 *****************************************************************************/

import * as Profiles from "./Profiles";
import * as Shapes from "./Shapes";
import type { Fun2Num, LookUp, SelectionDesc } from "./types";
import { isActive, toolMode, toolProfile, toolShape } from "./Window";

// array includes
function includes(array: any[], value: any): boolean {
    for (const val of array)
        if (val === value)
            return true;
    return false;
}

type Num4 = [number, number, number, number];

// Re-using a buffer for surface retrieval
let surfaceBuffer: Uint8Array = new Uint8Array(0);

function getSurfaces(selection: SelectionDesc): void {
    if (selection.tiles.length === 0) {
        surfaceBuffer = new Uint8Array(0);
        return;
    }
    surfaceBuffer = (map as any).getSurfaces(selection.binaryTiles);
}

function getSurfaceZFromBuffer(idx: number): Num4 {
    if (!surfaceBuffer || idx * 16 >= surfaceBuffer.length) return [0, 0, 0, 0];

    // SurfaceElement layout:
    // 0-4: TileElementBase (Type, Flags, BaseHeight, ClearanceHeight, Owner)
    // 5: Slope
    // 6: WaterHeight
    const baseHeight = surfaceBuffer[idx * 16 + 2];
    const slope = surfaceBuffer[idx * 16 + 5];

    return [0, 1, 2, 3].map(corner =>
        baseHeight / 2 // baseHeight in terrain steps
        + (slope >> corner & 1) // add 1 if corner is raised
        + (slope >> 4 & 1) // add 1 if slope is diagonally raised
        * (1 - (slope >> ((corner + 2) & 3) & 1)) // but only if the opposite corner is lowered
    ) as Num4;
}

function getBaseHeightFromBuffer(idx: number): number {
    if (!surfaceBuffer || idx * 16 >= surfaceBuffer.length) return 0;
    return surfaceBuffer[idx * 16 + 2];
}

function getSlopeFromBuffer(idx: number): number {
    if (!surfaceBuffer || idx * 16 >= surfaceBuffer.length) return 0;
    return surfaceBuffer[idx * 16 + 5];
}

function executeAll(tiles: CoordsXY[], fun: (tile: CoordsXY, idx: number) => LandSetHeightArgs | undefined): boolean {
    if (tiles.length === 0) return true;

    if (network.mode === "client") {
        const group = network.getGroup(network.currentPlayer.group);
        if (!includes(group.permissions, "terraform")) {
            ui.showError(
                context.formatString("{STRINGID}", 5637),
                context.formatString("{STRINGID}", 5638),
            );
            return false;
        }
    }

    const updatesCount = tiles.length;
    // Each update in binary: x(4), y(4), height(1), style(1) = 10 bytes
    const buffer = new Uint8Array(updatesCount * 10);
    let validUpdates = 0;

    for (let i = 0; i < updatesCount; i++) {
        const args = fun(tiles[i], i);
        if (args) {
            const offset = validUpdates * 10;
            const dv = new DataView(buffer.buffer, buffer.byteOffset + offset, 10);
            dv.setInt32(0, args.x, true);
            dv.setInt32(4, args.y, true);
            buffer[offset + 8] = args.height;
            buffer[offset + 9] = args.style;
            validUpdates++;
        }
    }

    if (validUpdates === 0) return true;
    const finalBuffer = validUpdates === updatesCount ? buffer : buffer.slice(0, validUpdates * 10);

    context.queryAction("landsetheight", { updates: finalBuffer }, result => {
        if (result.error) {
            ui.showError(
                result.errorTitle || "",
                result.errorMessage || "",
            );
        } else {
            // insufficient funds
            const cost = result.cost || 0;
            if (cost > park.cash && !park.getFlag("noMoney")) {
                ui.showError(
                    context.formatString("{STRINGID}", 5637),
                    context.formatString("{STRINGID}", 827, cost),
                );
            } else {
                context.executeAction("landsetheight", { updates: finalBuffer });
            }
        }
    });

    return true;
}

class Profile<T = Num4> {
    private readonly initialZ: (idx: number) => T;
    private data: LookUp<T> = {};

    public constructor(initialZ: (idx: number) => T) {
        this.initialZ = initialZ;
    }

    public setZ(idx: number, z: T): void {
        this.data[idx] = z;
    }

    public getZ(idx: number): T {
        if (this.data[idx])
            return this.data[idx];
        const z = this.initialZ(idx);
        this.setZ(idx, z);
        return z;
    }

    public lazyClone(): Profile<T> {
        return new Profile<T>(idx => this.getZ(idx));
    }
}

let currentProfile = new Profile(idx => getSurfaceZFromBuffer(idx));
let originalProfile = currentProfile.lazyClone();
let currentSelection: SelectionDesc | undefined;

export function softReset(): void {
    originalProfile = currentProfile.lazyClone();
}

export function hardReset(): void {
    if (currentSelection) {
        getSurfaces(currentSelection);
    }
    currentProfile = new Profile(idx => getSurfaceZFromBuffer(idx));
    softReset();
}

let strategy: Fun2Num = () => 0;
let tiles: CoordsXY[] = [];
let deltaProfile: (x: number, y: number) => number = () => 0;

export function setSelection(selectionDesc: SelectionDesc): void {
    currentSelection = selectionDesc;
    strategy = getStrategy(selectionDesc);
    tiles = selectionDesc.tiles;
    const profileFun: Fun2Num = (x, y) => Profiles[toolProfile.get()](Math.min(Shapes[toolShape.get()](x, y), 1));
    deltaProfile = (x, y) => {
        const rel = selectionDesc.transformation(x, y);
        return profileFun(rel.x, rel.y);
    };
    getSurfaces(selectionDesc);
    hardReset();
}

const cornerOffsets = [[1, 1], [1, 0], [0, 0], [0, 1]];
export function apply(delta: number): void {
    const changedProfile = currentProfile.lazyClone();
    if (executeAll(tiles, ({ x, y }, idx) => {
        const oldProfile = originalProfile.getZ(idx);
        const newProfile = cornerOffsets.map(([dx, dy], cornerIdx) => {
            const z = strategy(oldProfile[cornerIdx], deltaProfile(x + dx, y + dy) * delta);
            return Math.max(0, Math.min(127, z));
        }) as Num4;
        changedProfile.setZ(idx, newProfile);

        const integral = newProfile.map(corner => Math.round(corner));
        const height = Math.max(Math.min(...integral, 0x7F), 1);
        const relativeIntegral = integral.map(corner => Math.max(Math.min(corner, 0x7f) - height, 0));
        const slope = relativeIntegral.reduce((slope, z, idx) => {
            if (z) slope |= 1 << idx;
            if (z > 1) slope |= (1 << 4);
            return slope;
        }, 0);

        return {
            x: x << 5,
            y: y << 5,
            height: height << 1,
            style: slope,
        };
    }))
        currentProfile = changedProfile;
}

function getStrategy(selectionDesc: SelectionDesc): Fun2Num {
    switch (toolMode.get()) {
        case "relative":
            return (surface, delta) => surface + delta;
        case "absolute":
            let minZ = 255, maxZ = 0;
            for (let i = 0; i < selectionDesc.tiles.length; i++) {
                let surfaceZ = getBaseHeightFromBuffer(i);
                if (surfaceZ) {
                    surfaceZ >>= 1;
                    minZ = Math.min(minZ, surfaceZ);
                    maxZ = Math.max(maxZ, surfaceZ);
                }
            }
            return (surface, delta) => delta < 0 ? Math.min(surface, maxZ + delta) : Math.max(surface, minZ + delta);
        case "plateau":
            const { x, y } = selectionDesc.center;
            let targetZ = 0;
            const centerIdx = selectionDesc.tiles.findIndex(t => t.x === x && t.y === y);
            if (centerIdx !== -1) {
                targetZ = getBaseHeightFromBuffer(centerIdx) >> 1;
            }
            return (surface, delta) => {
                switch (true) {
                    case delta < 0 && targetZ < surface:
                        return Math.max(targetZ, surface + delta);
                    case delta > 0 && targetZ > surface:
                        return Math.min(targetZ, surface + delta);
                    default:
                        return surface;
                }
            };
    }
}

export function init(): void {
    isActive.subscribe(value => value && hardReset());
}

export function smooth(selection: SelectionDesc, delta: number): void {
    setSelection(selection);
    const fun1 = delta > 0 ? Math.max : Math.min;
    const fun2 = delta > 0 ? Math.min : Math.max;

    const cornerHeightsCache: LookUp<Num4> = {};
    for (let i = 0; i < tiles.length; i++) {
        cornerHeightsCache[i] = originalProfile.getZ(i);
    }

    executeAll(tiles, ({ x, y }, i) => {
        const oldProfile = cornerHeightsCache[i];

        const neighborHeights = cornerOffsets.map(([dx, dy], cornerIdx) => {
            const nx = x + dx;
            const ny = y + dy;
            const cornerHeights = cornerOffsets
                .map(([ndx, ndy], nIdx) => {
                    const tx = nx - ndx;
                    const ty = ny - ndy;
                    const nTileIdx = tiles.findIndex(t => t.x === tx && t.y === ty);
                    return nTileIdx !== -1 ? cornerHeightsCache[nTileIdx][nIdx] : undefined;
                })
                .filter(z => z !== undefined) as number[];
            return fun1(...cornerHeights);
        });

        const newProfile = cornerOffsets.map((_, cornerIdx) => {
            const z = fun2(oldProfile[cornerIdx] + delta, neighborHeights[cornerIdx]);
            return Math.max(0, Math.min(127, z));
        }) as Num4;

        const integral = newProfile.map(corner => Math.round(corner));
        const height = Math.max(Math.min(...integral, 0x7F), 1);
        const relativeIntegral = integral.map(corner => Math.max(Math.min(corner, 0x7f) - height, 0));
        const slope = relativeIntegral.reduce((slope, z, idx) => {
            if (z) slope |= 1 << idx;
            if (z > 1) slope |= (1 << 4);
            return slope;
        }, 0);

        return {
            x: x << 5,
            y: y << 5,
            height: height << 1,
            style: slope,
        };
    });
}

export function flat(selection: SelectionDesc, delta: number): void {
    setSelection(selection);
    executeAll(tiles, ({ x, y }, i) => {
        let height = getBaseHeightFromBuffer(i);
        const slope = getSlopeFromBuffer(i);
        if (delta > 0) {
            if (slope)
                height += 2;
            if (slope & 0x10)
                height += 2;
        }
        height = Math.max(0, Math.min(254, height));
        return {
            x: x << 5,
            y: y << 5,
            height: height,
            style: 0,
        };
    });
}

export function rough(selection: SelectionDesc): void {
    setSelection(selection);
    executeAll(tiles, ({ x, y }, i) => {
        return {
            x: x << 5,
            y: y << 5,
            height: getBaseHeightFromBuffer(i),
            style: Math.floor(Math.random() * 15),
        };
    });
}
