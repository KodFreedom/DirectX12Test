#pragma once

class RenderSystem;

RenderSystem* GetRenderSystem();
void ThrowIfFailed(HRESULT hr);