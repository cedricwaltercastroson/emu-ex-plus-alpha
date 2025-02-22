#pragma once

/*  This file is part of EmuFramework.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with EmuFramework.  If not, see <http://www.gnu.org/licenses/> */

#include <imagine/base/MessagePort.hh>
#include <imagine/thread/Thread.hh>
#include <variant>

namespace EmuEx
{

using namespace IG;

class EmuVideo;
class EmuAudio;
class EmuApp;

class EmuSystemTask
{
public:
	enum class Command: uint8_t
	{
		UNSET,
		RUN_FRAME,
		PAUSE,
		EXIT,
	};

	struct RunFrameCommand
	{
		EmuVideo *video{};
		EmuAudio *audio{};
		int8_t frames{};
		bool skipForward{};
		bool fastForward{};
	};

	struct PauseCommand {};
	struct ExitCommand {};

	using CommandVariant = std::variant<RunFrameCommand, PauseCommand, ExitCommand>;

	struct CommandMessage
	{
		std::binary_semaphore *semPtr{};
		CommandVariant command{RunFrameCommand{}};

		void setReplySemaphore(std::binary_semaphore *semPtr_) { assert(!semPtr); semPtr = semPtr_; };
	};

	EmuSystemTask(EmuApp &);
	void start();
	void pause();
	void stop();
	void runFrame(EmuVideo *, EmuAudio *, int8_t frames, bool skipForward, bool fastForward);
	void sendVideoFormatChangedReply(EmuVideo &);
	void sendFrameFinishedReply(EmuVideo &);
	void sendScreenshotReply(bool success);
	auto threadId() const { return threadId_; }

private:
	EmuApp &app;
	MessagePort<CommandMessage> commandPort{"EmuSystemTask Command"};
	std::thread taskThread;
	ThreadId threadId_{};
};

}
