#include "stdafx.h"
#include "SongTimer.hpp"
#include "SongTimerRead.hpp"

float SongTimer::SongTimer() {
	if (GameState::Menus::IsInPreSongTuner()) {
		return 0.f;
	}

	// Preserve the rare-timer fallback for songs such as Desolate Motion, but
	// never dereference a pointer obtained before another unchecked read.
	return Detail::ReadSongTime(GameState::Menus::IsInSongModes(),
		Offsets::baseHandle + Offsets::ptr_timer, Offsets::ptr_timerBaseOffsets,
		Offsets::baseHandle + Offsets::ptr_timerRare, Offsets::ptr_timerRareOffsets,
		[](uintptr_t address, auto& value) { return MemUtil::TryRead(address, value); });
}

/// <summary>
/// When the user passes a note in the pause menu, notes become grey. This gets the time at which notes go from being grey (deactivated) to being colored (activated).
/// </summary>
/// <returns>Time where all notes before it are grey / deactivated.</returns>
float SongTimer::GetGreyNoteTimer() {
	uintptr_t greyNoteTimer = MemUtil::FindDMAAddy(Offsets::baseHandle + Offsets::ptr_greyOutNoteTimer, Offsets::ptr_greyOutNoteTimerOffsets);

	if (!greyNoteTimer) {
		LOG_ERROR("Invalid Pointer: GetGreyNoteTimer = NULL" << std::endl);
		return NULL;
	}

	return *(float*)greyNoteTimer;
}

/// <summary>
/// Sets the time at which all notes before it turn grey (deactivated) and all notes after it are colorful (activated)
/// </summary>
/// <param name="timeInSeconds"> - Time to set the "deactivate before" at.</param>
void SongTimer::SetGreyNoteTimer(float timeInSeconds) {
	uintptr_t greyNoteTimer = MemUtil::FindDMAAddy(Offsets::baseHandle + Offsets::ptr_greyOutNoteTimer, Offsets::ptr_greyOutNoteTimerOffsets);

	if (!greyNoteTimer) {
		LOG_ERROR("Invalid Pointer: SetGreyNoteTimer = NULL" << std::endl);
		return;
	}

	*(float*)greyNoteTimer = timeInSeconds;
}

/// <summary>
/// Gets how long the countdown will last for from start to end, between songs, in NSP.
/// </summary>
/// <returns>The amount of time it takes to go from song 1 to song 2.</returns>
double SongTimer::GetNonStopPlayTimer()
{
	return *(double*)Offsets::ptr_NonStopPlayPreSongTimer.Get();
}

/// <summary>
/// Sets the amount of time between song 1 and song 2 in NSP.
/// </summary>
/// <param name="NewTimer"> - New time to set</param>
void SongTimer::SetNonStopPlayTimer(double NewTimer)
{
	MemUtil::SetStaticValue(Offsets::ptr_NonStopPlayPreSongTimer, NewTimer, sizeof(NewTimer));
}
