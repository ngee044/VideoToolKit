#pragma once

#include "ThreadPool.h"

#include <string>
#include <queue>
#include <functional>
#include <tuple>
#include <optional>
#include <memory>
#include <mutex>

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct AVCodec;
struct AVCodeParameters;
struct SwsContext;

using namespace Thread;

namespace VideoToolKit
{
	// TODO
	enum class PixelFormat
	{ 
		Auto,
		YUV,
		RGB 
	};

	class VideoCapture
	{
	public:
		VideoCapture();
		~VideoCapture();

		auto open(const std::string& source_url, bool use_gpu = false, PixelFormat output_format = PixelFormat::Auto) -> std::tuple<bool, std::optional<std::string>>;
		auto close() -> void;

		auto stop_capture() -> void;
		
		auto start_capture_async(std::function<void(AVFrame*)> callback) -> std::tuple<bool, std::optional<std::string>>;

		auto grab_frame(AVFrame** output_frame) -> std::tuple<bool, std::optional<std::string>>;
		auto get_frame_async(AVFrame** frame) -> std::tuple<bool, std::optional<std::string>>;

	protected:
		auto create_thread_pool() -> std::tuple<bool, std::optional<std::string>>;
		auto destroy_thread_pool() -> std::tuple<bool, std::optional<std::string>>;

	private:
		void capture_loop(std::function<void(AVFrame*)> callback);
		
		int video_stream_index_;
		bool use_gpu_;
		bool is_capturing_;
		PixelFormat output_format_;
		AVFormatContext* format_context_;
		AVCodecContext* codec_context_;
		AVFrame* frame_yuv_;
		AVFrame* frame_rgb_;
		AVPacket* packet_;

		SwsContext* sws_context_;

		std::shared_ptr<ThreadPool> thread_pool_;

		std::queue<AVFrame*> frame_queue_;
		std::mutex frame_queue_mutex_;
		std::condition_variable frame_queue_cond_;
		std::atomic_bool stop_capture_;


	};

}