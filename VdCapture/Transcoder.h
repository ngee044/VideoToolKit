#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext_cuda.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <cuda.h>
#include <cuda_runtime.h>

#include <vector>
#include <tuple>
#include <string>
#include <optional>
#include <memory>

namespace VideoToolKit
{
	class Transcoder
	{
	public:
		struct OutputConfig
		{
			std::string url;
			int width;
			int height;
			int bit_rate;
			std::string format;
		};

		Transcoder();
		~Transcoder();

		auto initialize(const std::vector<OutputConfig>& output_configs, int frame_rate = 30) -> std::tuple<bool, std::optional<std::string>>;
		auto process_frame(const std::uint8_t* gpu_y, const std::uint8_t* gpu_uv, int stride_y, int stride_uv) -> std::tuple<bool, std::optional<std::string>>;

		auto finalize() -> void;

		
	protected:
		struct StreamContext
		{
			OutputConfig config;
			AVFormatContext* format_context_ = nullptr;
			AVCodecContext* codec_context_ = nullptr;
			AVStream* stream_ = nullptr;
			
			SwsContext* sws_context_ = nullptr;
			AVFrame* tmp_frame_ = nullptr;
		};

		auto init_cuda_device() -> std::tuple<bool, std::optional<std::string>>;
		auto setup_encoder(StreamContext& stream_context, int fps) -> std::tuple<bool, std::optional<std::string>>;
		auto setup_muxer(StreamContext& stream_context) -> std::tuple<bool, std::optional<std::string>>;
		auto send_frame(StreamContext& stream_context, AVFrame* frame) -> std::tuple<bool, std::optional<std::string>>;

		auto scale_frame_cpu(const std::uint8_t* gpu_y, const std::uint8_t* gpu_uv, int stride_y, int stride_uv, StreamContext& stream_context) -> std::tuple<bool, std::optional<std::string>>;

	private:
		std::vector<StreamContext> stream_contexts_;

		AVBufferRef* hw_device_context_;;
		std::int64_t pts_;
		int fps_;
	};
}