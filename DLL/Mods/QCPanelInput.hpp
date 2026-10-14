#pragma once
namespace QCAutomation::Panel {
bool WantsInputCapture();
namespace Input {
bool Initialize();
bool Ready();
void BeforeFrame();
void Clear();
void EndCapture();
}
}
