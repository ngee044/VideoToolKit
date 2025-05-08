#include "VideoCapture.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
}

#include "Converter.h"
#include "ThreadWorker.h"
#include "JobPriorities.h"
#include "Job.h"
#include "JobPool.h"


#include "fmt/format.h"


namespace VideoToolKit
{

VideoCapture::VideoCapture() :
	use_gpu_(false),
	output_format_(PixelFormat::Auto),
	format_context_(nullptr),
	video_stream_index_(-1),
	thread_pool_(nullptr),
	codec_context_(nullptr),
	frame_yuv_(nullptr),
	frame_rgb_(nullptr),
	packet_(nullptr),
	sws_context_(nullptr),
	is_capturing_(false)
{
	avdevice_register_all();
}

VideoCapture::~VideoCapture() 
{
	close();
}

auto VideoCapture::open(const std::string& source_url, bool use_gpu, PixelFormat output_format) -> std::tuple<bool, std::optional<std::string>> 
{
	auto [created, create_error] = create_thread_pool();
	if (!created)
	{
		return { false, create_error };
	}

	if (source_url.empty()) 
	{
		return { false, "Source URL is empty" };
	}
	
	use_gpu_ = use_gpu;
	output_format_ = output_format;

	if (avformat_open_input(&format_context_, source_url.c_str(), nullptr, nullptr) < 0) 
	{
		return { false, "Failed to open video source" };
	}

	if (avformat_find_stream_info(format_context_, nullptr) < 0) 
	{
		avformat_close_input(&format_context_);
		return { false, "Failed to find stream info" };
	}

	AVCodecParameters* codec_parameters = nullptr;
	for (auto i = 0; i < format_context_->nb_streams; ++i)
	{
		if (format_context_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) 
		{
			video_stream_index_ = i;
			codec_parameters = format_context_->streams[i]->codecpar;
			break;
		}
	}

	if (video_stream_index_ < 0)
	{
		return { false, "No video stream found" };
	}

	const AVCodec* decoder = avcodec_find_decoder(codec_parameters->codec_id);
	if (!decoder) 
	{
		return { false, "Failed to find decoder" };
	}

	codec_context_ = avcodec_alloc_context3(decoder);
	if (!codec_context_) 
	{
		return { false, "Failed to allocate codec context" };
	}

	if (avcodec_parameters_to_context(codec_context_, codec_parameters) < 0) 
	{
		return { false, "Failed to copy codec parameters to context" };
	}
	
	// Set GPU options if needed
	if (use_gpu_)
	{
		AVHWDeviceType device_type = AV_HWDEVICE_TYPE_CUDA;
		AVBufferRef* hw_device_context = nullptr;
		if (av_hwdevice_ctx_create(&hw_device_context, device_type, nullptr, nullptr, 0) < 0) 
		{
			use_gpu_ = false;
		}
		else
		{
			codec_context_->hw_device_ctx = av_buffer_ref(hw_device_context);
		}
	}

	if (avcodec_open2(codec_context_, decoder, nullptr) < 0) 
	{
		avcodec_free_context(&codec_context_);
		return { false, "Failed to open codec" };
	}

	packet_ = av_packet_alloc();
	frame_yuv_ = av_frame_alloc();
	if (!packet_ || !frame_yuv_) 
	{
		avcodec_free_context(&codec_context_);
		return { false, "Failed to allocate packet or frame" };
	}

	if (output_format_ == PixelFormat::RGB) 
	{
		frame_rgb_ = av_frame_alloc();
		if (!frame_rgb_) 
		{
			av_packet_free(&packet_);
			av_frame_free(&frame_yuv_);
			avcodec_free_context(&codec_context_);
			return { false, "Failed to allocate RGB frame" };
		}

		frame_rgb_->format = AV_PIX_FMT_RGB24;
		frame_rgb_->width = codec_context_->width;
		frame_rgb_->height = codec_context_->height;

		if (av_frame_get_buffer(frame_rgb_, 0) < 0) 
		{
			av_frame_free(&frame_rgb_);
			av_packet_free(&packet_);
			av_frame_free(&frame_yuv_);
			avcodec_free_context(&codec_context_);
			return { false, "Failed to allocate RGB frame buffer" };
		}

		sws_context_ = sws_getContext(codec_context_->width, codec_context_->height, codec_context_->pix_fmt,
			frame_rgb_->width, frame_rgb_->height, AV_PIX_FMT_RGB24,
			SWS_BILINEAR, nullptr, nullptr, nullptr);

		if (!sws_context_)
		{
			av_frame_free(&frame_rgb_);
			av_packet_free(&packet_);
			av_frame_free(&frame_yuv_);
			avcodec_free_context(&codec_context_);
			return { false, "Failed to create SWS context" };
		}
	}

	return { true, std::nullopt };
}

auto VideoCapture::close() -> void
{
	stop_capture();
	
	destroy_thread_pool();

	if (frame_rgb_)
	{
		av_frame_free(&frame_rgb_);
		frame_rgb_ = nullptr;
	}

	if (frame_yuv_)
	{
		av_frame_free(&frame_yuv_);
		frame_yuv_ = nullptr;
	}

	if (packet_)
	{
		av_packet_free(&packet_);
		packet_ = nullptr;
	}

	if (codec_context_) 
	{
		avcodec_free_context(&codec_context_);
		codec_context_ = nullptr;
	}
	
	if (format_context_) 
	{
		avformat_close_input(&format_context_);
		format_context_ = nullptr;
	}

	if (sws_context_) 
	{
		sws_freeContext(sws_context_);
		sws_context_ = nullptr;
	}

	video_stream_index_ = -1;
	use_gpu_ = false;
	output_format_ = PixelFormat::Auto;
}

auto VideoCapture::stop_capture() -> void 
{
	if (is_capturing_)
	{
		is_capturing_ = false;

		// Wait for the capture thread to finish
#if 0
		if (thread_pool_)
		{
			auto job_pool = thread_pool_->job_pool();
			while (job_pool->job_count(JobPriorities::High) > 0)
			{
				// std::this_thread::sleep_for(std::chrono::milliseconds(100));
				// std::this_thread::yield();
				// write log
			}
		}
#endif

	}

	{
		std::lock_guard<std::mutex> lock(frame_queue_mutex_);
		while (!frame_queue_.empty())
		{
			AVFrame* frame = frame_queue_.front();
			frame_queue_.pop();
			av_frame_free(&frame);
		}
	}

}

auto VideoCapture::start_capture_async(std::function<void(AVFrame*)> callback) -> std::tuple<bool, std::optional<std::string>>
{
	if (!format_context_ || !codec_context_)
	{
		return { false, "VideoCapture not opened" };
	}

	if (!is_capturing_)
	{
		return { false, "Capture already started" };
	}

	is_capturing_ = true;

	auto [started, start_error] = thread_pool_->push(std::make_shared<Job>(JobPriorities::High, 
		[&, callback]() -> std::tuple<bool, std::optional<std::string>> 
		{ 
			capture_loop(callback);
			return { true, std::nullopt };
		}));

	if (!started)
	{
		is_capturing_ = false;
		return { false, start_error };
	}
	return { true, std::nullopt };
}

auto VideoCapture::grab_frame(AVFrame** output_frame) -> std::tuple<bool, std::optional<std::string>> 
{ 
	if (!format_context_ || !codec_context_)
	{
		return { false, "VideoCapture not opened" };
	}

	AVFrame* frame = nullptr;
	bool frame_ready = false;

	while (!frame_ready)
	{
		if (av_read_frame(format_context_, packet_) < 0)
		{
			// End of stream(EOF) or error
			return { false, "Failed to read frame" };
		}

		if (packet_->stream_index != video_stream_index_)
		{
			av_packet_unref(packet_);
			continue;
		}

		if (avcodec_send_packet(codec_context_, packet_) < 0)
		{
			av_packet_unref(packet_);
			continue;
		}

		int ret = avcodec_receive_frame(codec_context_, frame_yuv_);
		if (ret == 0)
		{
			frame_ready = true;

			if (output_format_ == PixelFormat::RGB)
			{
				sws_scale(sws_context_, frame_yuv_->data, frame_yuv_->linesize, 0, codec_context_->height,
					frame_rgb_->data, frame_rgb_->linesize);
				
				// frame = frame_rgb_;
				AVFrame* rgb_out = av_frame_alloc();
				av_frame_copy_props(rgb_out, frame_rgb_);
				rgb_out->format = frame_rgb_->format;
				rgb_out->width = frame_rgb_->width;
				rgb_out->height = frame_rgb_->height;

				av_frame_get_buffer(rgb_out, 0);
#if 0
				av_image_copy(rgb_out->data, rgb_out->linesize, frame_rgb_->data, frame_rgb_->linesize,
					AV_PIX_FMT_RGB24, frame_rgb_->width, frame_rgb_->height);
#else
				av_image_copy(rgb_out->data, rgb_out->linesize, frame_rgb_->data, frame_rgb_->linesize,
					(AVPixelFormat)frame_rgb_->format, frame_rgb_->width, frame_rgb_->height);
#endif			
				frame = rgb_out;
			}
			else
			{
				AVFrame* yuv_out = av_frame_alloc();
				av_frame_ref(yuv_out, frame_yuv_);
				frame = frame_yuv_;
			}
		}

		av_packet_unref(packet_);
	}

	*output_frame = frame;

	return { true, std::nullopt };
}

auto VideoCapture::get_frame_async(AVFrame** frame) -> std::tuple<bool, std::optional<std::string>> 
{
	std::unique_lock<std::mutex> lock(frame_queue_mutex_);
	
	if (frame_queue_.empty())
	{
		return { false, "No frame available" };
	}

	*frame = frame_queue_.front();
	frame_queue_.pop();

	return { true, std::nullopt };
}

auto VideoCapture::create_thread_pool() -> std::tuple<bool, std::optional<std::string>> 
{
	auto [destroyed, destroy_error] = destroy_thread_pool();
	if (!destroyed)
	{
		return { false, destroy_error };
	}

	try
	{
		thread_pool_ = std::make_shared<ThreadPool>();
	}
	catch (const std::bad_alloc& e)
	{
		return { false, fmt::format("thread pool creation failed: {}", e.what()) };
	}

	thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::Normal }));

	return { true, std::nullopt };
}

auto VideoCapture::destroy_thread_pool() -> std::tuple<bool, std::optional<std::string>> 
{
	if (thread_pool_ == nullptr)
	{
		return { true, std::nullopt };
	}

	auto [stopped, stop_error] = thread_pool_->stop();
	if (!stopped)
	{
		return { false, stop_error };
	}

	thread_pool_.reset();

	return { true, std::nullopt };
}

void VideoCapture::capture_loop(std::function<void(AVFrame*)> callback)
{
	while (is_capturing_)
	{
		AVFrame* frame = nullptr;

		auto [grabbed, grab_error] = grab_frame(&frame);
		if (!grabbed)
		{
			// TODO
			// write error to log
			break;
		}

		if (callback)
		{
			callback(frame);
		}
		else
		{
			{
				std::lock_guard<std::mutex> lock(frame_queue_mutex_);
				frame_queue_.push(frame);
			}
			frame_queue_cond_.notify_one();
		}
	}
	is_capturing_ = false;
}

}