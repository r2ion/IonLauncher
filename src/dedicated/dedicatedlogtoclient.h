#pragma once
#include "logging/logging.h"
#include "core/convar/convar.h"

class DedicatedServerLogToClientSink : public spdlog::sinks::base_sink<std::mutex>
{
protected:
	void sink_it_(const spdlog::details::log_msg& msg) override;
	void flush_() override;
};
