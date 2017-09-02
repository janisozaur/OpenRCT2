#include "lighting.h"
#include "../world/map.h"
#include "../world/scenery.h"
#include "../world/footpath.h"

// how the light will be affected when light passes through a certain plane
lighting_value lightingAffectorsX[LIGHTMAP_SIZE_Y][LIGHTMAP_SIZE_X][LIGHTMAP_SIZE_Z];
lighting_value lightingAffectorsY[LIGHTMAP_SIZE_Y][LIGHTMAP_SIZE_X][LIGHTMAP_SIZE_Z];
lighting_value lightingAffectorsZ[LIGHTMAP_SIZE_Y][LIGHTMAP_SIZE_X][LIGHTMAP_SIZE_Z];

// lightmap columns whose lightingAffectors are outdated
// each element contains a bitmap, each bit identifying if a direction should be recomputed (4 bits, 1 << MAP_ELEMENT_DIRECTION_... for each direction)
// Z is always recomputed
uint8 affectorRecomputeQueue[LIGHTMAP_SIZE_Y][LIGHTMAP_SIZE_X];

lighting_chunk lightingChunks[LIGHTMAP_CHUNKS_Z][LIGHTMAP_CHUNKS_Y][LIGHTMAP_CHUNKS_X];

const lighting_value black = { .r = 0,.g = 0,.b = 0 };
const lighting_value ambient = { .r = 0,.g = 0,.b = 0 };
const lighting_value ambient_sky = { .r = 20,.g = 20,.b = 20 };
const lighting_value lit = { .r = 255,.g = 255,.b = 255 };

#define SUBCELLITR(v, cbidx) for (int v = (cbidx); v < (cbidx) + 2; v++)

// multiplies @target light with some multiplier light value @apply
static void lighting_multiply(lighting_value* target, const lighting_value apply) {
	target->r *= apply.r / 255.0f;
	target->g *= apply.g / 255.0f;
	target->b *= apply.b / 255.0f;
}

// adds @target light with some multiplier light value @apply
static void lighting_add(lighting_value* target, const lighting_value apply) {
	// TODO: can overflow
	target->r += apply.r;
	target->g += apply.g;
	target->b += apply.b;
}

static void lighting_muladd(lighting_value* target, const lighting_value apply, const float multiplier) {
    // TODO: can overflow
    target->r += multiplier * apply.r;
    target->g += multiplier * apply.g;
    target->b += multiplier * apply.b;
}

// elementswise lerp between @a and @b depending on @frac (@lerp = 0 -> @a)
// @return 
static lighting_value interpolate_lighting(const lighting_value a, const lighting_value b, float frac) {
	return (lighting_value) { 
		.r = a.r * (1.0f - frac) + b.r * frac,
		.g = a.g * (1.0f - frac) + b.g * frac,
		.b = a.b * (1.0f - frac) + b.b * frac
	};
}

// cast a ray from a 3d world position @a (light source position) to lighting tile
// @return 
static lighting_value FASTCALL lighting_raycast(lighting_value color, const rct_xyz32 light_source_pos, const rct_xyz16 lightmap_texel) {
	float x = light_source_pos.x / 16.0f;
	float y = light_source_pos.y / 16.0f;
	float z = light_source_pos.z / 2.0f;
	float dx = (lightmap_texel.x + .5) - x;
	float dy = (lightmap_texel.y + .5) - y;
	float dz = (lightmap_texel.z + .5) - z;

	for (int px = min(lightmap_texel.x+1, (int)ceil(x)); px <= max((int)floor(x), lightmap_texel.x); px++) {
		float multiplier = (px - x) / dx;
		float py = dy * multiplier + y - .5;
		float pz = dz * multiplier + z - .5;
		int affectorY = py;
		int affectorZ = pz;
		float progy = py - affectorY;
		float progz = pz - affectorZ;
		lighting_value v00 = lightingAffectorsX[affectorY][px][affectorZ];
		lighting_value v01 = lightingAffectorsX[affectorY+1][px][affectorZ];
		lighting_value v10 = lightingAffectorsX[affectorY][px][affectorZ + 1];
		lighting_value v11 = lightingAffectorsX[affectorY+1][px][affectorZ + 1];
		lighting_value v0 = interpolate_lighting(v00, v01, progy);
		lighting_value v1 = interpolate_lighting(v10, v11, progy);
		lighting_multiply(&color, interpolate_lighting(v0, v1, progz));
	}

	for (int py = min(lightmap_texel.y + 1, (int)ceil(y)); py <= max((int)floor(y), lightmap_texel.y); py++) {
		float multiplier = (py - y) / dy;
		float px = dx * multiplier + x - .5;
		float pz = dz * multiplier + z - .5;
		int affectorX = px;
		int affectorZ = pz;
		float progx = px - affectorX;
		float progz = pz - affectorZ;
		lighting_value v00 = lightingAffectorsY[py][affectorX][affectorZ];
		lighting_value v01 = lightingAffectorsY[py][affectorX + 1][affectorZ];
		lighting_value v10 = lightingAffectorsY[py][affectorX][affectorZ + 1];
		lighting_value v11 = lightingAffectorsY[py][affectorX + 1][affectorZ + 1];
		lighting_value v0 = interpolate_lighting(v00, v01, progx);
		lighting_value v1 = interpolate_lighting(v10, v11, progx);
		lighting_multiply(&color, interpolate_lighting(v0, v1, progz));
	}

	for (int pz = min(lightmap_texel.z + 1, (int)ceil(z)); pz <= max((int)floor(z), lightmap_texel.z); pz++) {
		float multiplier = (pz - z) / dz;
		float px = dx * multiplier + x - .5;
		float py = dy * multiplier + y - .5;
		int affectorX = px;
		int affectorY = py;
		float progx = px - affectorX;
		float progy = py - affectorY;
		lighting_value v00 = lightingAffectorsZ[affectorY][affectorX][pz];
		lighting_value v01 = lightingAffectorsZ[affectorY][affectorX + 1][pz];
		lighting_value v10 = lightingAffectorsZ[affectorY + 1][affectorX][pz];
		lighting_value v11 = lightingAffectorsZ[affectorY + 1][affectorX + 1][pz];
		lighting_value v0 = interpolate_lighting(v00, v01, progx);
		lighting_value v1 = interpolate_lighting(v10, v11, progx);
		lighting_multiply(&color, interpolate_lighting(v0, v1, progy));
	}

	return color;
}

// inserts a static light into the chunks this light can reach
static void lighting_insert_static_light(const lighting_light light) {
	int range = 11;
	sint32 lm_x = light.map_x * 2;
	sint32 lm_y = light.map_y * 2;
	sint32 lm_z = light.pos.z / 2;
	for (int sz = max(0, (lm_z - range * 2) / LIGHTMAP_CHUNK_SIZE); sz <= min(LIGHTMAP_CHUNKS_Z - 1, (lm_z + range * 2) / LIGHTMAP_CHUNK_SIZE); sz++) {
		for (int sy = max(0, (lm_y - range) / LIGHTMAP_CHUNK_SIZE); sy <= min(LIGHTMAP_CHUNKS_Y - 1, (lm_y + range) / LIGHTMAP_CHUNK_SIZE); sy++) {
			for (int sx = max(0, (lm_x - range) / LIGHTMAP_CHUNK_SIZE); sx <= min(LIGHTMAP_CHUNKS_X - 1, (lm_x + range) / LIGHTMAP_CHUNK_SIZE); sx++) {
				lighting_chunk* chunk = &lightingChunks[sz][sy][sx];
				// TODO: bounds check
				chunk->static_lights[chunk->static_lights_count++] = light;
			}
		}
	}
}

void lighting_invalidate_at(sint32 wx, sint32 wy) {
	// remove static lights at this position
	// iterate chunks lights could reach, find lights in this column, remove them
	int range = 11;
	sint32 lm_x = wx * LIGHTING_CELL_SUBDIVISIONS;
	sint32 lm_y = wy * LIGHTING_CELL_SUBDIVISIONS;
	for (int sz = 0; sz < LIGHTMAP_CHUNKS_Z; sz++) {
		for (int sy = max(0, (lm_y - range) / LIGHTMAP_CHUNK_SIZE); sy <= min(LIGHTMAP_CHUNKS_Y - 1, (lm_y + range) / LIGHTMAP_CHUNK_SIZE); sy++) {
			for (int sx = max(0, (lm_x - range) / LIGHTMAP_CHUNK_SIZE); sx <= min(LIGHTMAP_CHUNKS_X - 1, (lm_x + range) / LIGHTMAP_CHUNK_SIZE); sx++) {
				lighting_chunk* chunk = &lightingChunks[sz][sy][sx];
				for (size_t lidx = 0; lidx < chunk->static_lights_count; lidx++) {
					if (chunk->static_lights[lidx].map_x == wx && chunk->static_lights[lidx].map_y == wy) {
						chunk->static_lights[lidx] = chunk->static_lights[chunk->static_lights_count - 1];
						chunk->static_lights_count--;
						lidx--;
					}
				}
				chunk->invalid = true;
			}
		}
	}

	// iterate column
	rct_map_element* map_element = map_get_first_element_at(wx, wy);
	if (map_element) {
		do {
			switch (map_element_get_type(map_element)) {
				case MAP_ELEMENT_TYPE_PATH: {
					if (footpath_element_has_path_scenery(map_element) && !(map_element->flags & MAP_ELEMENT_FLAG_BROKEN)) {
						// test
						rct_scenery_entry *sceneryEntry = get_footpath_item_entry(footpath_element_get_path_scenery_index(map_element));
						sint32 x = wx * 32 + 16;
						sint32 y = wy * 32 + 16;
						if (sceneryEntry->path_bit.flags & PATH_BIT_FLAG_LAMP) {
							int z = map_element->base_height * 2 + 6;
							if (!(map_element->properties.path.edges & (1 << 0))) {
								lighting_insert_static_light((lighting_light) { .map_x = wx, .map_y = wy, .pos = { .x = x - 14, .y = y, .z = z }, .color = lit });
							}
							if (!(map_element->properties.path.edges & (1 << 1))) {
								lighting_insert_static_light((lighting_light) { .map_x = wx, .map_y = wy, .pos = { .x = x, .y = y + 14, .z = z }, .color = lit });
							}
							if (!(map_element->properties.path.edges & (1 << 2))) {
								lighting_insert_static_light((lighting_light) { .map_x = wx, .map_y = wy, .pos = { .x = x + 14, .y = y, .z = z }, .color = lit });
							}
							if (!(map_element->properties.path.edges & (1 << 3))) {
								lighting_insert_static_light((lighting_light) { .map_x = wx, .map_y = wy, .pos = { .x = x, .y = y - 14, .z = z }, .color = lit });
							}
						}
					}
					break;
				}
			}
		} while (!map_element_is_last_for_tile(map_element++));
	}

	// revert values to lit...
	for (int lm_z = 0; lm_z < LIGHTMAP_SIZE_Z; lm_z++) {
        for (int ulm_x = lm_x; ulm_x <= lm_x + LIGHTING_CELL_SUBDIVISIONS; ulm_x++) {
            SUBCELLITR(sy, lm_y) lightingAffectorsX[sy][ulm_x][lm_z] = lit;
        }
        for (int ulm_y = lm_y; ulm_y <= lm_y + LIGHTING_CELL_SUBDIVISIONS; ulm_y++) {
            SUBCELLITR(sx, lm_x) lightingAffectorsY[ulm_y][sx][lm_z] = lit;
		}
        SUBCELLITR(sy, lm_y) SUBCELLITR(sx, lm_x) lightingAffectorsZ[sy][sx][lm_z] = lit;
	}

	// queue rebuilding affectors
    SUBCELLITR(sy, lm_y) affectorRecomputeQueue[sy][lm_x] = 0b1111;
    SUBCELLITR(sx, lm_x) affectorRecomputeQueue[lm_y][sx] = 0b1111;

	if (lm_x > 0) { // east
        SUBCELLITR(sy, lm_y) affectorRecomputeQueue[sy][lm_x - 1] |= 0b0100;
	}
	if (lm_y > 0) { // north
        SUBCELLITR(sx, lm_x) affectorRecomputeQueue[lm_y - 1][sx] |= 0b0010;
	}
	if (lm_x < LIGHTMAP_SIZE_X - 2) { // east
        SUBCELLITR(sy, lm_y) affectorRecomputeQueue[sy][lm_x + LIGHTING_CELL_SUBDIVISIONS] |= 0b0001;
	}
	if (lm_y < LIGHTMAP_SIZE_Y - 2) { // south
        SUBCELLITR(sx, lm_x) affectorRecomputeQueue[lm_y + LIGHTING_CELL_SUBDIVISIONS][sx] |= 0b1000;
	}
}

void lighting_invalidate_around(sint32 wx, sint32 wy) {
	lighting_invalidate_at(wx, wy);
	if (wx < LIGHTMAP_SIZE_X - 1) lighting_invalidate_at(wx + 1, wy);
	if (wy < LIGHTMAP_SIZE_Y - 1) lighting_invalidate_at(wx, wy + 1);
	if (wx > 0) lighting_invalidate_at(wx - 1, wy);
	if (wy > 0) lighting_invalidate_at(wx, wy - 1);
}

void lighting_init() {
	// reset affectors to 1^3
	for (int z = 0; z < LIGHTMAP_SIZE_Z; z++) {
		for (int y = 0; y < LIGHTMAP_SIZE_Y; y++) {
			for (int x = 0; x < LIGHTMAP_SIZE_X; x++) {
				lightingAffectorsX[y][x][z] = lit;
				lightingAffectorsY[y][x][z] = lit;
				lightingAffectorsZ[y][x][z] = lit;
			}
		}
	}

	// init chunks
	for (int z = 0; z < LIGHTMAP_CHUNKS_Z; z++) {
		for (int y = 0; y < LIGHTMAP_CHUNKS_Y; y++) {
			for (int x = 0; x < LIGHTMAP_CHUNKS_X; x++) {
				lightingChunks[z][y][x].invalid = true;
				lightingChunks[z][y][x].static_lights_count = 0;
				lightingChunks[z][y][x].x = x;
				lightingChunks[z][y][x].y = y;
				lightingChunks[z][y][x].z = z;
			}
		}
	}

	lighting_reset();
}

void lighting_invalidate_all() {
	// invalidate/recompute all columns ((re)loads all lights on the map)
	for (int y = 0; y < MAXIMUM_MAP_SIZE_PRACTICAL-1; y++) {
		for (int x = 0; x < MAXIMUM_MAP_SIZE_PRACTICAL-1; x++) {
			lighting_invalidate_at(x, y);
		}
	}
}

#pragma region Colormap
// stolen from opengldrawingengine
static const float TransparentColourTable[144 - 44][3] =
{
	{ 0.7f, 0.8f, 0.8f }, // 44
	{ 0.7f, 0.8f, 0.8f },
	{ 0.3f, 0.4f, 0.4f },
	{ 0.2f, 0.3f, 0.3f },
	{ 0.1f, 0.2f, 0.2f },
	{ 0.4f, 0.5f, 0.5f },
	{ 0.3f, 0.4f, 0.4f },
	{ 0.4f, 0.5f, 0.5f },
	{ 0.4f, 0.5f, 0.5f },
	{ 0.3f, 0.4f, 0.4f },
	{ 0.6f, 0.7f, 0.7f },
	{ 0.3f, 0.5f, 0.9f },
	{ 0.1f, 0.3f, 0.8f },
	{ 0.5f, 0.7f, 0.9f },
	{ 0.6f, 0.2f, 0.2f },
	{ 0.5f, 0.1f, 0.1f },
	{ 0.8f, 0.4f, 0.4f },
	{ 0.3f, 0.5f, 0.4f },
	{ 0.2f, 0.4f, 0.2f },
	{ 0.5f, 0.7f, 0.5f },
	{ 0.5f, 0.5f, 0.7f },
	{ 0.3f, 0.3f, 0.5f },
	{ 0.6f, 0.6f, 0.8f },
	{ 0.5f, 0.5f, 0.2f },
	{ 0.4f, 0.4f, 0.1f },
	{ 0.7f, 0.7f, 0.4f },
	{ 0.7f, 0.5f, 0.3f },
	{ 0.6f, 0.4f, 0.2f },
	{ 0.8f, 0.7f, 0.4f },
	{ 0.8f, 0.7f, 0.1f },
	{ 0.7f, 0.4f, 0.0f },
	{ 1.0f, 0.9f, 0.2f },
	{ 0.4f, 0.6f, 0.2f },
	{ 0.3f, 0.4f, 0.2f },
	{ 0.5f, 0.7f, 0.3f },
	{ 0.5f, 0.6f, 0.4f },
	{ 0.4f, 0.4f, 0.3f },
	{ 0.7f, 0.8f, 0.5f },
	{ 0.3f, 0.7f, 0.2f },
	{ 0.2f, 0.6f, 0.0f },
	{ 0.4f, 0.8f, 0.3f },
	{ 0.8f, 0.5f, 0.4f },
	{ 0.7f, 0.4f, 0.3f },
	{ 0.9f, 0.7f, 0.5f },
	{ 0.5f, 0.3f, 0.7f },
	{ 0.4f, 0.2f, 0.6f },
	{ 0.7f, 0.5f, 0.8f },
	{ 0.9f, 0.0f, 0.0f },
	{ 0.7f, 0.0f, 0.0f },
	{ 1.0f, 0.3f, 0.3f },
	{ 1.0f, 0.4f, 0.1f },
	{ 0.9f, 0.3f, 0.0f },
	{ 1.0f, 0.6f, 0.3f },
	{ 0.2f, 0.6f, 0.6f },
	{ 0.0f, 0.4f, 0.4f },
	{ 0.4f, 0.7f, 0.7f },
	{ 0.9f, 0.2f, 0.6f },
	{ 0.6f, 0.1f, 0.4f },
	{ 1.0f, 0.5f, 0.7f },
	{ 0.6f, 0.5f, 0.4f },
	{ 0.4f, 0.3f, 0.2f },
	{ 0.7f, 0.7f, 0.6f },
	{ 0.9f, 0.6f, 0.6f },
	{ 0.8f, 0.5f, 0.5f },
	{ 1.0f, 0.7f, 0.7f },
	{ 0.7f, 0.8f, 0.8f },
	{ 0.5f, 0.6f, 0.6f },
	{ 0.9f, 1.0f, 1.0f },
	{ 0.2f, 0.3f, 0.3f },
	{ 0.4f, 0.5f, 0.5f },
	{ 0.7f, 0.8f, 0.8f },
	{ 0.2f, 0.3f, 0.5f },
	{ 0.5f, 0.5f, 0.7f },
	{ 0.5f, 0.3f, 0.7f },
	{ 0.1f, 0.3f, 0.7f },
	{ 0.3f, 0.5f, 0.9f },
	{ 0.6f, 0.8f, 1.0f },
	{ 0.2f, 0.6f, 0.6f },
	{ 0.5f, 0.8f, 0.8f },
	{ 0.1f, 0.5f, 0.0f },
	{ 0.3f, 0.5f, 0.4f },
	{ 0.4f, 0.6f, 0.2f },
	{ 0.3f, 0.7f, 0.2f },
	{ 0.5f, 0.6f, 0.4f },
	{ 0.5f, 0.5f, 0.2f },
	{ 1.0f, 0.9f, 0.2f },
	{ 0.8f, 0.7f, 0.1f },
	{ 0.6f, 0.3f, 0.0f },
	{ 1.0f, 0.4f, 0.1f },
	{ 0.7f, 0.3f, 0.0f },
	{ 0.7f, 0.5f, 0.3f },
	{ 0.5f, 0.3f, 0.1f },
	{ 0.5f, 0.4f, 0.3f },
	{ 0.8f, 0.5f, 0.4f },
	{ 0.6f, 0.2f, 0.2f },
	{ 0.6f, 0.0f, 0.0f },
	{ 0.9f, 0.0f, 0.0f },
	{ 0.6f, 0.1f, 0.3f },
	{ 0.9f, 0.2f, 0.6f },
	{ 0.9f, 0.6f, 0.6f },
};
#pragma endregion Colormap

static void lighting_static_light_cast(lighting_value* target_value, lighting_light light, sint32 px, sint32 py, sint32 pz) {
	sint32 range = 11;
	sint32 w_x = px * 16 + 8;
	sint32 w_y = py * 16 + 8;
	sint32 w_z = pz * 2 + 1;
	float distpot = sqrt((w_x - light.pos.x)*(w_x - light.pos.x) + (w_y - light.pos.y)*(w_y - light.pos.y) + (w_z - light.pos.z)*(w_z - light.pos.z) * 4 * 4);
	float intensity = 1.0f - distpot / (range * 12.0f);
	if (intensity > 0) {
		rct_xyz16 target = { .x = px,.y = py,.z = pz };
		intensity *= 10;
		lighting_value source_value = { .r = intensity,.g = intensity,.b = intensity };
		lighting_add(target_value, lighting_raycast(source_value, light.pos, target));
	}
}

static void lighting_update_affectors() {
	for (int y = 0; y < LIGHTMAP_SIZE_Y; y++) {
		for (int x = 0; x < LIGHTMAP_SIZE_X; x++) {
			uint8 dirs = affectorRecomputeQueue[y][x];
			if (dirs) {
				rct_map_element* map_element = map_get_first_element_at(x / LIGHTING_CELL_SUBDIVISIONS, y / LIGHTING_CELL_SUBDIVISIONS);
				if (map_element) {
					do {
						switch (map_element_get_type(map_element))
						{
						case MAP_ELEMENT_TYPE_SURFACE: {
							for (int z = 0; z < map_element->base_height - 1; z++) {
								lightingAffectorsX[y][x][z] = black;
								lightingAffectorsY[y][x][z] = black;
								lightingAffectorsX[y][x + 1][z] = black;
								lightingAffectorsY[y + 1][x][z] = black;
								lightingAffectorsZ[y][x][z] = black;
								lightingAffectorsZ[y][x][z + 1] = black;
							}
							break;
						}
						case MAP_ELEMENT_TYPE_SCENERY: {
							for (int z = map_element->base_height - 1; z < map_element->clearance_height - 1; z++) {
								lightingAffectorsX[y][x][z] = black;
								lightingAffectorsY[y][x][z] = black;
								lightingAffectorsX[y][x + 1][z] = black;
								lightingAffectorsY[y + 1][x][z] = black;
								lightingAffectorsZ[y][x][z] = black;
								lightingAffectorsZ[y][x][z + 1] = black;
							}
							break;
						}
						case MAP_ELEMENT_TYPE_WALL: {
							// do not apply if the wall its direction is not queued
							if (!(dirs & (1 << map_element_get_direction(map_element)))) {
								break;
							}
							for (int z = map_element->base_height - 1; z < map_element->clearance_height; z++) {
								lighting_value affector = black;
								if (map_element->properties.wall.type == 54) {
									uint8 color = map_element->properties.wall.colour_1;
									if (color >= 44 && color < 144) {
										affector.r = TransparentColourTable[color - 44][0] * 255;
										affector.g = TransparentColourTable[color - 44][1] * 255;
										affector.b = TransparentColourTable[color - 44][2] * 255;
									}
								}
								switch (map_element_get_direction(map_element)) {
								case MAP_ELEMENT_DIRECTION_NORTH:
									if (y % LIGHTING_CELL_SUBDIVISIONS == LIGHTING_CELL_SUBDIVISIONS - 1) lighting_multiply(&lightingAffectorsY[y + LIGHTING_CELL_SUBDIVISIONS - 1][x][z], affector);
									break;
								case MAP_ELEMENT_DIRECTION_SOUTH:
									if (y % LIGHTING_CELL_SUBDIVISIONS == 0) lighting_multiply(&lightingAffectorsY[y][x][z], affector);
									break;
								case MAP_ELEMENT_DIRECTION_EAST:
									if (x % LIGHTING_CELL_SUBDIVISIONS == LIGHTING_CELL_SUBDIVISIONS - 1) lighting_multiply(&lightingAffectorsX[y][x + LIGHTING_CELL_SUBDIVISIONS - 1][z], affector);
									break;
								case MAP_ELEMENT_DIRECTION_WEST:
									if (x % LIGHTING_CELL_SUBDIVISIONS == 0) lighting_multiply(&lightingAffectorsX[y][x][z], affector);
									break;
								}
							}
							break;
						}
						}
					} while (!map_element_is_last_for_tile(map_element++));
				}
				affectorRecomputeQueue[y][x] = 0;
			}
		}
	}
}

static void lighting_update_chunk(lighting_chunk* chunk) {
    // reset skylight
    // chunk->skylight_carry is used to store the lighting value at the current layer (i.e. `oz` in the next loop)
    // at the end of that loop, skylight_carry will be the carry for the chunk below it too
    if (chunk->z == LIGHTMAP_CHUNKS_Z - 1) {
        // top chunk, skylight = lit
        for (int oy = 0; oy < LIGHTMAP_CHUNK_SIZE; oy++) {
            for (int ox = 0; ox < LIGHTMAP_CHUNK_SIZE; ox++) {
                chunk->skylight_carry[oy][ox] = ambient_sky;
            }
        }
    }
    else {
        // not top chunk, copy skylight from the chunk above
        memcpy(chunk->skylight_carry, lightingChunks[chunk->z + 1][chunk->y][chunk->x].skylight_carry, sizeof(chunk->skylight_carry));
    }

	for (int oz = LIGHTMAP_CHUNK_SIZE - 1; oz >= 0; oz--) {
		for (int oy = 0; oy < LIGHTMAP_CHUNK_SIZE; oy++) {
			for (int ox = 0; ox < LIGHTMAP_CHUNK_SIZE; ox++) {
                chunk->data[oz][oy][ox] = ambient;

                // skylight
                lighting_value affector = lightingAffectorsZ[chunk->y*LIGHTMAP_CHUNK_SIZE + oy][chunk->x*LIGHTMAP_CHUNK_SIZE + ox][chunk->z*LIGHTMAP_CHUNK_SIZE + oz];
                lighting_multiply(&chunk->skylight_carry[oy][ox], affector);

                chunk->data[oz][oy][ox] = chunk->skylight_carry[oy][ox];

                // static lights that reach this chunk
				for (size_t lidx = 0; lidx < chunk->static_lights_count; lidx++) {
					lighting_static_light_cast(&chunk->data[oz][oy][ox], chunk->static_lights[lidx], chunk->x*LIGHTMAP_CHUNK_SIZE + ox, chunk->y*LIGHTMAP_CHUNK_SIZE + oy, chunk->z*LIGHTMAP_CHUNK_SIZE + oz);
				}
			}
		}
	}
	chunk->invalid = false;
}

static void lighting_update_static(lighting_update_batch* updated_batch) {
    // TODO: this is not monotonic on Windows
    clock_t max_end = clock() + LIGHTING_MAX_CLOCKS_PER_FRAME;

    // recompute invalid chunks until reaching a limit
    // start from the top to pass through skylights in the correct order
    for (int z = LIGHTMAP_CHUNKS_Z - 1; z >= 0; z--) {
        for (int y = 0; y < LIGHTMAP_CHUNKS_Y; y++) {
            for (int x = 0; x < LIGHTMAP_CHUNKS_X; x++) {
                lightingChunks[z][y][x].has_dynamic_lights = false;

                if (lightingChunks[z][y][x].invalid) {
                    // recompute this invalid chunk
                    lighting_chunk* chunk = &lightingChunks[z][y][x];
                    lighting_update_chunk(chunk);
                    updated_batch->updated_chunks[updated_batch->update_count++] = chunk;

                    // exceeding max update count?
                    if (updated_batch->update_count >= LIGHTING_MAX_CHUNK_UPDATES_PER_FRAME) {
                        return;
                    }

                    // exceeding max time?
                    if (clock() > max_end) {
                        return;
                    }
                }
            }
        }
    }
}

static lighting_value* lighting_get_dynamic_texel(lighting_update_batch* updated_batch, int x, int y, int z) {
    int lm_z = z / LIGHTMAP_CHUNK_SIZE;
    int lm_y = y / LIGHTMAP_CHUNK_SIZE;
    int lm_x = x / LIGHTMAP_CHUNK_SIZE;

    if (lm_z < 0 || lm_y < 0 || lm_x < 0 || lm_z >= LIGHTMAP_CHUNKS_Z || lm_y >= LIGHTMAP_CHUNKS_Y || lm_x >= LIGHTMAP_CHUNKS_X) return NULL;

    lighting_chunk* chunk = &lightingChunks[lm_z][lm_y][lm_x];
    if (!chunk->has_dynamic_lights) {
        memcpy(chunk->data_dynamic, chunk->data, sizeof(chunk->data));
        chunk->has_dynamic_lights = true;

        updated_batch->updated_chunks[updated_batch->update_count++] = chunk;
    }

    return &chunk->data_dynamic[z % LIGHTMAP_CHUNK_SIZE][y % LIGHTMAP_CHUNK_SIZE][x % LIGHTMAP_CHUNK_SIZE];
}

static void lighting_add_dynamic(lighting_update_batch* updated_batch, sint16 x, sint16 y, sint16 z) {
    int lm_x = (x * LIGHTING_CELL_SUBDIVISIONS) / 32;
    int lm_y = (y * LIGHTING_CELL_SUBDIVISIONS) / 32;
    int lm_z = z / 8;
    int range = 8;
    for (int pz = lm_z - range; pz <= lm_z + range; pz++) {
        for (int py = lm_y - range; py <= lm_y + range; py++) {
            for (int px = lm_x - range; px <= lm_x + range; px++) {
                if (updated_batch->update_count >= LIGHTING_MAX_CHUNK_UPDATES_PER_FRAME) return;
                lighting_value* texel = lighting_get_dynamic_texel(updated_batch, px, py, pz);
                if (texel) {
                    sint32 w_x = px * (32 / LIGHTING_CELL_SUBDIVISIONS) + (16 / LIGHTING_CELL_SUBDIVISIONS);
                    sint32 w_y = py * (32 / LIGHTING_CELL_SUBDIVISIONS) + (16 / LIGHTING_CELL_SUBDIVISIONS);
                    sint32 w_z = pz * 8 + 1;
                    float distpot = sqrt((w_x - x)*(w_x - x) + (w_y - y)*(w_y - y) + (w_z - z)*(w_z - z));

                    float intensity = 1.0f - distpot / (range * 16.0f);
                    //log_info("int %f", intensity);
                    if (intensity > 0) {
                        rct_xyz32 pos = { .x = x,.y = y,.z = z / 4 };
                        rct_xyz16 target = { .x = px,.y = py,.z = pz };
                        intensity *= 70;
                        lighting_value source_value = { .r = intensity,.g = intensity,.b = intensity };
                        lighting_add(texel, lighting_raycast(source_value, pos, target));
                    }
                }
            }
        }
    }
}

static void lighting_update_dynamic(lighting_update_batch* updated_batch) {
    // TODO: this is not monotonic on Windows
    //clock_t max_end = clock() + LIGHTING_MAX_CLOCKS_PER_FRAME;

    //log_info("reg");

    uint16 spriteIndex = gSpriteListHead[SPRITE_LIST_TRAIN];
    while (spriteIndex != SPRITE_INDEX_NULL) {
        rct_vehicle * vehicle = &(get_sprite(spriteIndex)->vehicle);
        uint16 vehicleID = spriteIndex;
        spriteIndex = vehicle->next;

        if (vehicle->ride_subtype == RIDE_ENTRY_INDEX_NULL) {
            continue;
        }

        for (uint16 q = vehicleID; q != SPRITE_INDEX_NULL; ) {
            vehicle = GET_VEHICLE(q);

            vehicleID = q;
            if (vehicle->next_vehicle_on_train == q)
                break;
            q = vehicle->next_vehicle_on_train;

            sint16 place_x, place_y, place_z;

            place_x = vehicle->x;
            place_y = vehicle->y;
            place_z = vehicle->z;

            rct_ride *ride = get_ride(vehicle->ride);
            switch (ride->type) {
            case RIDE_TYPE_MONORAIL:
            case RIDE_TYPE_LOOPING_ROLLER_COASTER:
                if (vehicle == vehicle_get_head(vehicle)) {
                    lighting_add_dynamic(updated_batch, place_x, place_y, place_z);
                }
                break;
            case RIDE_TYPE_BOAT_RIDE:
                lighting_add_dynamic(updated_batch, place_x, place_y, place_z);
                break;
            default:
                break;
            };
        }
    }

}

static lighting_update_batch lighting_update_internal() {
	// update all pending affectors first
	lighting_update_affectors();

    lighting_update_batch updated_batch = { .update_count = 0 };

    lighting_update_static(&updated_batch);
    lighting_update_dynamic(&updated_batch);

	updated_batch.updated_chunks[updated_batch.update_count] = NULL;

	return updated_batch;
}

void lighting_reset() {
	for (int y = 0; y < MAXIMUM_MAP_SIZE_PRACTICAL; y++) {
		for (int x = 0; x < MAXIMUM_MAP_SIZE_PRACTICAL; x++) {
			affectorRecomputeQueue[y][x] = 0b1111;
		}
	}
}

lighting_update_batch lighting_update() {
	return lighting_update_internal();
}
