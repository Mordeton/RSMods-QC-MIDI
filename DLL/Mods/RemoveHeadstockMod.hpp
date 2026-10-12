#pragma once

#include "../Framework/GamePhase.hpp"
#include "../Framework/Framework.hpp"

class RemoveHeadstockMod : public Framework::IMod {
public:
    MOD_ID(RemoveHeadstockMod)

    bool IsEnabled(const Framework::ModContext& c) const override;

    void OnEnabled(Framework::ModContext& c) override;
    void OnDisabled(Framework::ModContext& c) override;
    void OnSettingsChanged(Framework::ModContext& c) override;
    void OnSongEnter(Framework::ModContext& c) override;
    void OnSongExit(Framework::ModContext& c) override;

private:
    void SyncState(Framework::ModContext& c);

    bool active = false;
};