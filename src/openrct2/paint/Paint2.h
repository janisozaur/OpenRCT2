#pragma once 

#include "paint.h"

#ifdef __cplusplus 
extern "C" {
#endif 

    void paint_viewport(rct_drawpixelinfo * dpi, uint32 viewFlags);
    paint_struct paint_session_arrange(paint_session *session);

#ifdef __cplusplus 
}
#endif 