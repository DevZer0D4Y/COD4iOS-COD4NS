#pragma once
#include <cmath>
// Only full-height, centered screen overlays qualify. Small HUD widgets,
// dialog panels and side-anchored graphics retain their original geometry.
inline bool ExpandCenteredScreenOverlay(float &x,float y,float &width,float height,
                                        float viewportWidth,float viewportHeight,float canvasWidth) {
    if(viewportWidth<=canvasWidth+1 || canvasWidth<=0 || viewportHeight<=0)return false;
    const float tolerance=2.0f;
    if(std::fabs(y)>tolerance || std::fabs(height-viewportHeight)>tolerance
       || width<canvasWidth-tolerance
       || std::fabs(x+width*.5f-viewportWidth*.5f)>tolerance)return false;
    x=0;width=viewportWidth;return true;
}
