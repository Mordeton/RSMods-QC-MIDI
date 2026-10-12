#pragma once

#include "../Framework/Framework.hpp"

class NonStopPlayTimerMod : public Framework::IMod {
public:
    MOD_ID(NonStopPlayTimerMod)

    void OnEnabled(Framework::ModContext& c) override;
    void OnDisabled(Framework::ModContext& c) override;
    void OnSettingsChanged(Framework::ModContext& c) override;

private:
    void ApplyTimer(Framework::ModContext& c);

    bool active = false;
};