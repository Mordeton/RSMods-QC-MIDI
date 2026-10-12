#pragma once

namespace Framework {
	enum class GamePhase {
		Loading, // Game still booting toward the main menu (memory is not safe to read freely).
		Menu,
		Song,
	};
}
