#include "Paint2.h"
#include "../interface/viewport.h"
#include "../config/Config.h"
#include <vector>
#include <algorithm>
#include <queue>

typedef struct paint_struct_bound_fbox {
    float x;
    float y;
    float z;
    float x_end;
    float y_end;
    float z_end;
} paint_struct_bound_fbox;

struct paint_struct_sorted
{
    std::vector<paint_struct_sorted*> behind;
    paint_struct_bound_fbox bounds;
    paint_struct *ps;
    bool visited;
    float depth;
    size_t isoDepth;
};

void visitNode(paint_struct_sorted& node, size_t& sortDepth)
{
    if (node.visited == true)
        return;

    for (size_t i = 0; i < node.behind.size(); i++)
    {
        if (node.behind[i] == nullptr)
            break;

        visitNode(*node.behind[i], sortDepth);

        node.behind[i] = nullptr;
    }

    node.visited = true;
    node.isoDepth = sortDepth++;
}

inline bool is_bbox_behind(uint8 rotation, const paint_struct_bound_fbox &a,
    const paint_struct_bound_fbox &b)
{
    constexpr const float padding = 0.0f;

    bool result = false;
    switch (rotation)
    {
    case 0:
        if (a.z_end + padding >= b.z - padding && 
            a.y_end + padding >= b.y - padding && 
            a.x_end + padding >= b.x - padding && 
            !(a.z + padding < b.z_end - padding && 
                a.y + padding < b.y_end - padding && 
                a.x + padding < b.x_end - padding))
            result = true;
        break;
    case 1:
        if (a.z_end + padding >= b.z - padding && 
            a.y_end + padding >= b.y - padding && 
            a.x_end + padding < b.x - padding && 
            !(a.z + padding < b.z_end - padding && 
                a.y + padding < b.y_end - padding &&
                a.x + padding >= b.x_end - padding))
            result = true;
        break;
    case 2:
        if (a.z_end + padding >= b.z - padding &&
            a.y_end + padding < b.y - padding &&
            a.x_end + padding < b.x - padding &&
            !(a.z + padding < b.z_end - padding &&
                a.y + padding >= b.y_end - padding &&
                a.x + padding >= b.x_end - padding))
            result = true;
        break;
    case 3:
        if (a.z_end + padding >= b.z - padding &&
            a.y_end + padding < b.y - padding &&
            a.x_end + padding >= b.x - padding &&
            !(a.z + padding < b.z_end - padding &&
                a.y + padding >= b.y_end - padding &&
                a.x + padding < b.x_end - padding))
            result = true;
        break;
    }
    return result;
}

extern "C"
void paint_sort_new(paint_session * session, uint8 rotation, std::vector<paint_struct_sorted>& group)
{
    group.clear();

    uint32 quadrantIndex = session->QuadrantBackIndex;
    if (quadrantIndex != UINT32_MAX) {
        do {
            paint_struct * ps_next = session->Quadrants[quadrantIndex];
            if (ps_next != NULL) {

                do {
                    paint_struct_sorted s;
                    s.visited = false;
                    s.bounds = {
                        (float)ps_next->bound_box_x,
                        (float)ps_next->bound_box_y,
                        (float)ps_next->bound_box_z,
                        (float)ps_next->bound_box_x_end,
                        (float)ps_next->bound_box_y_end,
                        (float)ps_next->bound_box_z_end,
                    };
                    s.ps = ps_next;
                    s.depth = (ps_next->bound_box_x + ps_next->bound_box_y) + (ps_next->bound_box_z_end * 1.25f);
                    s.isoDepth = 0;

                    group.emplace_back(s);
                    ps_next = ps_next->next_quadrant_ps;
                } while (ps_next != NULL);
            }
        } while (++quadrantIndex <= session->QuadrantFrontIndex);
    }


    size_t maxBehind = 0;

    for (size_t i = 0; i < group.size(); i++)
    {
        paint_struct_sorted& a = group[i];
        for (size_t j = 0; j < group.size(); j++)
        {
            if (i == j)
                continue;

            paint_struct_sorted& b = group[j];

            if (is_bbox_behind(rotation, a.bounds, b.bounds))
            {
                a.behind.push_back(&b);
                maxBehind = std::max(a.behind.size(), maxBehind);
            }
        }
    }

    size_t sortDepth = 0;

    for (size_t i = 0; i < group.size(); i++)
    {
        visitNode(group[i], sortDepth);
    }

    //log_info("Max behind: %zd", maxBehind);

    std::sort(group.begin(), group.end(), [](paint_struct_sorted& a, paint_struct_sorted& b)->bool
    {
        return a.isoDepth < b.isoDepth;
    });
}


/**
*
*  rct2: 0x00688485
*/
void paint_draw_structs(rct_drawpixelinfo * dpi, paint_struct * ps, uint32 viewFlags)
{
    paint_struct* previous_ps = ps;

    for (ps = ps->next_quadrant_ps; ps;)
    {
        sint16 x = ps->x;
        sint16 y = ps->y;
        if (ps->sprite_type == VIEWPORT_INTERACTION_ITEM_SPRITE) {
            if (dpi->zoom_level >= 1) {
                x = floor2(x, 2);
                y = floor2(y, 2);
                if (dpi->zoom_level >= 2) {
                    x = floor2(x, 4);
                    y = floor2(y, 4);
                }
            }
        }

        uint32 imageId = paint_ps_colourify_image(ps->image_id, ps->sprite_type, viewFlags);
        if (gPaintBoundingBoxes && dpi->zoom_level == 0) {
            paint_ps_image_with_bounding_boxes(dpi, ps, imageId, x, y);
        }
        else {
            paint_ps_image(dpi, ps, imageId, x, y);
        }

        if (ps->var_20 != 0) {
            ps = ps->var_20;
        }
        else {
            paint_attached_ps(dpi, ps, viewFlags);
            ps = previous_ps->next_quadrant_ps;
            previous_ps = ps;
        }
    }
}

void paint_draw_structs_new(rct_drawpixelinfo * dpi, const std::vector<paint_struct_sorted>& groups, uint32 viewFlags)
{
    paint_struct* previous_ps = nullptr;

    //for (ps = ps->next_quadrant_ps; ps;) {
    for (const auto& itr : groups) 
    {
        paint_struct *ps = itr.ps;
        if (previous_ps == nullptr)
            previous_ps = ps;

        sint16 x = ps->x;
        sint16 y = ps->y;
        if (ps->sprite_type == VIEWPORT_INTERACTION_ITEM_SPRITE) {
            if (dpi->zoom_level >= 1) {
                x = floor2(x, 2);
                y = floor2(y, 2);
                if (dpi->zoom_level >= 2) {
                    x = floor2(x, 4);
                    y = floor2(y, 4);
                }
            }
        }

        uint32 imageId = paint_ps_colourify_image(ps->image_id, ps->sprite_type, viewFlags);
        if (gPaintBoundingBoxes && dpi->zoom_level == 0) {
            paint_ps_image_with_bounding_boxes(dpi, ps, imageId, x, y);
        }
        else {
            paint_ps_image(dpi, ps, imageId, x, y);
        }

        if (ps->var_20 != 0) {
            ps = ps->var_20;
        }
        else {
            paint_attached_ps(dpi, ps, viewFlags);
            ps = previous_ps->next_quadrant_ps;
            previous_ps = ps;
        }
    }
}

extern "C"
void paint_viewport(rct_drawpixelinfo * dpi, uint32 viewFlags)
{
    gCurrentViewportFlags = viewFlags;

    if (viewFlags & (VIEWPORT_FLAG_HIDE_VERTICAL | VIEWPORT_FLAG_HIDE_BASE | VIEWPORT_FLAG_UNDERGROUND_INSIDE | VIEWPORT_FLAG_PAINT_CLIP_TO_HEIGHT)) {
        uint8 colour = 10;
        if (viewFlags & VIEWPORT_FLAG_INVISIBLE_SPRITES) {
            colour = 0;
        }
        gfx_clear(dpi, colour);
    }

    static std::vector<paint_struct_sorted> groups;

    paint_session * session = paint_session_alloc(dpi);
    paint_session_generate(session);
    
    paint_sort_new(session, get_current_rotation(), groups);
    paint_draw_structs_new(dpi, groups, viewFlags);
    paint_session_free(session);

    if (gConfigGeneral.render_weather_gloom &&
        !gTrackDesignSaveMode &&
        !(viewFlags & VIEWPORT_FLAG_INVISIBLE_SPRITES)
        ) {
        viewport_paint_weather_gloom(dpi);
    }

    if (session->PSStringHead != NULL) {
        paint_draw_money_structs(dpi, session->PSStringHead);
    }
}

extern "C"
paint_struct paint_session_arrange(paint_session *session)
{
    static std::vector<paint_struct_sorted> group;

    paint_sort_new(session, get_current_rotation(), group);

    paint_struct head = { 0 };
    if (group.size() > 0)
    {
        auto itr = group.begin();
        head.next_quadrant_ps = (*itr).ps;
        while (itr != group.end())
        {
            auto itrNext = itr + 1;
            if (itrNext == group.end())
                (*itr).ps->next_quadrant_ps = nullptr;
            else
                (*itr).ps->next_quadrant_ps = (*itrNext).ps;
            itr = itrNext;
        }
    }
    return head;
}
