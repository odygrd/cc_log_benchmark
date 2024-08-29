#include <atomic>
#include <filesystem>
#include <thread>
#include <vector>
#include <mutex>
#include "log_msg/log_msg.h"

#define MIN_TIME 0
#include "gbenchmark/log_gbenchmark.h"

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/sinks/FileSink.h"

struct FrontendOptionsUnbounded
{
    // UnboundedUnlimited will lead memory increase, can't finish in my laptop
    static constexpr quill::QueueType queue_type = quill::QueueType::UnboundedBlocking;
    static constexpr uint32_t initial_queue_capacity = 8 * 1024 * 1024;
    static constexpr uint32_t blocking_queue_retry_interval_ns = 800;
    static constexpr bool huge_pages_enabled = false;
};

using logger_t = quill::LoggerImpl<FrontendOptionsUnbounded>;
using frontend_t = quill::FrontendImpl<FrontendOptionsUnbounded>;

std::once_flag init_flag;

logger_t* logger = frontend_t::create_or_get_logger(
        "file_logger", frontend_t::create_or_get_sink<quill::FileSink>(
                "logs/gbenchmark_quill_bounded.log",
                []()
                {
                    quill::FileSinkConfig cfg;
                    cfg.set_open_mode('w');
                    cfg.set_filename_append_option(quill::FilenameAppendOption::None);
                    return cfg;
                }(),
                quill::FileEventNotifier{}),
        quill::PatternFormatterOptions{"[%(log_level)]|[%(time)]|[%(file_name):%(line_number)]|[%(caller_function)]|[%(thread_id)] - %(message)",
                                       "%H:%M:%S.%Qns", quill::Timezone::GmtTime});

#define LOG_FUNC(num)                                                 \
	void log_func##num(LogMsg &msg)                                   \
	{                                                                 \
		LOG_INFO(logger, "u64: {}, i64: {}, u32: {}, i32: {}, s: {}", \
				 (unsigned long long)msg.u64, (long long)msg.i64,     \
				 (unsigned long)msg.u32, (long)msg.i32, msg.s);       \
	}

EXPAND_FUNCS

class QuillUnboundedFixture : public benchmark::Fixture {
public:
	QuillUnboundedFixture()
	{
		if (!LoadLogMsg(log_msgs)) {
			fprintf(stderr,
					"failed load random data, Run gen_random_data first");
			exit(EXIT_FAILURE);
		}
	}

	void SetUp(const benchmark::State &)
	{
        std::call_once(init_flag, []() {
            std::filesystem::create_directories("logs");
            quill::Backend::start();

            logger->set_log_level(quill::LogLevel::TraceL3);
            logger->init_backtrace(2u, quill::LogLevel::Critical);
        });
	}

public:
	std::vector<LogMsg> log_msgs;
};

// min time
RUN_GBENCHMARK(QuillUnboundedFixture, write)
