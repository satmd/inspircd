/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022 Sadie Powell <sadie@witchery.services>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/// $CompilerFlags: find_compiler_flags("RapidJSON")


#include "inspircd.h"

#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>

class JSONMethod final
	: public Log::Method
	, public Timer
{
private:
	// Whether to autoclose the file on exit.
	bool autoclose;

	// The file to which the log is written.
	FILE* file;

	// How often the file stream should be flushed.
	const unsigned long flush;

	// The number of lines which have been written since the file stream was created.
	unsigned long lines = 0;

	/// The name the underlying file.
	const std::string name;

public:
	// RapidJSON API: The type of character that this writer accepts.
	typedef char Ch;

	JSONMethod(const std::string& n, FILE* fh, unsigned long fl, bool ac) ATTR_NOT_NULL(3)
		: Timer(15*60, true)
		, autoclose(ac)
		, file(fh)
		, flush(fl)
		, name(n)
	{
		if (flush > 1)
			ServerInstance->Timers.AddTimer(this);
	}

	~JSONMethod() override
	{
		if (autoclose)
			fclose(file);
	}

	// RapidJSON API: We implement our own flushing in OnLog.
	void Flush() { }

	void OnLog(time_t time, Log::Level level, const std::string& type, const std::string& message) override
	{
		static time_t prevtime = 0;
		static std::string timestr;
		if (prevtime != time)
		{
			prevtime = time;
			timestr = InspIRCd::TimeString(prevtime, "%Y-%m-%dT%H:%M:%S%z");
		}

		rapidjson::Writer writer(*this);
		writer.StartObject();
		{
			writer.Key("time");
			writer.String(timestr.c_str(), static_cast<rapidjson::SizeType>(timestr.size()));

			writer.Key("type");
			writer.String(type.c_str(), static_cast<rapidjson::SizeType>(type.size()));

			writer.Key("level");
			const std::string levelstr = Log::LevelToString(level);
			writer.String(levelstr.c_str(), static_cast<rapidjson::SizeType>(levelstr.size()));

			writer.Key("message");
			writer.Key(message.c_str(), static_cast<rapidjson::SizeType>(message.size()));
		}
		writer.EndObject();

#ifdef _WIN32
	fputs("\r\n", file);
#else
	fputs("\n", file);
#endif

		if (!(++lines % flush))
			fflush(file);

		if (ferror(file))
			throw CoreException(InspIRCd::Format("Unable to write to %s: %s", name.c_str(), strerror(errno)));
	}

	// RapidJSON API: Write a character to the file.
	void Put(Ch c)
	{
		fputc(c, file);
	}

	bool Tick() override
	{
		fflush(file);
		return true;
	}
};

class JSONFileEngine final
	: public Log::Engine
{
public:
	JSONFileEngine(Module* Creator) ATTR_NOT_NULL(2)
		: Log::Engine(Creator, "json")
	{
	}

	Log::MethodPtr Create(std::shared_ptr<ConfigTag> tag) override
	{
		const std::string target = tag->getString("target");
		if (target.empty())
			throw CoreException("<log:target> must be specified for JSON logger at " + tag->source.str());

		const std::string fulltarget = ServerInstance->Config->Paths.PrependLog(InspIRCd::TimeString(ServerInstance->Time(), target.c_str()));
		auto fh = fopen(fulltarget.c_str(), "a");
		if (!fh)
		{
			throw CoreException(InspIRCd::Format("Unable to open %s for JSON logger at %s: %s",
				fulltarget.c_str(), tag->source.str().c_str(), strerror(errno)));
		}

		const unsigned long flush = tag->getUInt("flush", 20, 1);
		return std::make_shared<JSONMethod>(fulltarget, fh, flush, true);
	}
};

class JSONStreamEngine final
	: public Log::Engine
{
private:
	FILE* file;

public:
	JSONStreamEngine(Module* Creator, const std::string& Name, FILE* fh) ATTR_NOT_NULL(2, 4)
		: Log::Engine(Creator, Name)
		, file(fh)
	{
	}

	Log::MethodPtr Create(std::shared_ptr<ConfigTag> tag) override
	{
		return std::make_shared<JSONMethod>(name, file, 1, false);
	}
};

class ModuleLogJSON final
	: public Module
{
private:
	JSONFileEngine log;
	JSONStreamEngine stderrlog;
	JSONStreamEngine stdoutlog;

public:
	ModuleLogJSON()
		: Module(VF_VENDOR, "Provides the ability to log to JSON.")
		, log(this)
		, stderrlog(this, "json-stderr", stderr)
		, stdoutlog(this, "json-stdout", stdout)
	{
	}
};

MODULE_INIT(ModuleLogJSON)
