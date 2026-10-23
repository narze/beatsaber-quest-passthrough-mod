#pragma once

namespace passthrough {

// Logs all cameras and enabled renderers (main thread only).
void DumpRenderState(const char *reason);

} // namespace passthrough
