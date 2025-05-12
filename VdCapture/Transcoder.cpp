#include "Transcoder.h"

#include <fmt/format.h>

#include <iostream>
#include <cassert>


namespace VideoToolKit
{

	Transcoder::Transcoder() :
		fps_(30),
		pts_(0),
		hw_device_context_(nullptr)
	{
		av_log_set_level(AV_LOG_WARNING);
		avformat_network_init();
	}

	Transcoder::~Transcoder() 
	{
		finalize();
		avformat_network_deinit();
	}

	auto Transcoder::init_cuda_device() -> std::tuple<bool, std::optional<std::string>> 
	{
		if (hw_device_context_) 
		{
			return { true, std::nullopt };
		}

		if (av_hwdevice_ctx_create(&hw_device_context_, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0) < 0)
		{
			return { false, "Failed to create CUDA device context" };
		}

		return { true, std::nullopt };
	}

	auto Transcoder::initialize(const std::vector<OutputConfig>& output_configs, int fps) -> std::tuple<bool, std::optional<std::string>> 
	{
		auto [init_cuda, cuda_error] = init_cuda_device();
		if (!init_cuda) 
		{
			return { false, cuda_error };
		}

		fps_ = fps;

		for (const auto& config : output_configs) 
		{
			StreamContext stream_context;
			stream_context.config = config;

			auto [setup_muxer, muxer_error] = setup_muxer(stream_context);
			if (!setup_muxer) 
			{
				return { false, muxer_error };
			}

			auto [setup_encoder, encoder_error] = setup_encoder(stream_context, fps);
			if (!setup_encoder) 
			{
				return { false, encoder_error };
			}

			stream_contexts_.push_back(stream_context);
		}

		return { true, std::nullopt };
	}

	auto Transcoder::process_frame(const std::uint8_t* gpu_y, const std::uint8_t* gpu_uv, int stride_y, int stride_uv) -> std::tuple<bool, std::optional<std::string>>
	{
		for (auto& stream_context : stream_contexts_)
		{
			auto [scale_frame, scale_error] = scale_frame_cpu(gpu_y, gpu_uv, stride_y, stride_uv, stream_context);
			
			if (!scale_frame)
			{
				return { false, scale_error };
			}

			auto [send_success, send_error] = send_frame(stream_context, stream_context.tmp_frame_);
			if (!send_success) 
			{
				return { false, send_error };
			}
		}
		++pts_;
		return { true, std::nullopt };
	}

	auto Transcoder::finalize() -> void 
	{
		for (auto& stream_context : stream_contexts_)
		{
			avcodec_send_frame(stream_context.codec_context_, nullptr);
			AVPacket packet;
			av_init_packet(&packet);
			while (avcodec_receive_packet(stream_context.codec_context_, &packet) == 0)
			{
				packet.stream_index = stream_context.stream_->index;
				av_interleaved_write_frame(stream_context.format_context_, &packet);
				av_packet_unref(&packet);
			}
			av_write_trailer(stream_context.format_context_);
			if (!(stream_context.format_context_->oformat->flags & AVFMT_NOFILE))
			{
				avio_closep(&stream_context.format_context_->pb);
			}

			sws_freeContext(stream_context.sws_context_);
			av_frame_free(&stream_context.tmp_frame_);
			avcodec_free_context(&stream_context.codec_context_);
			avformat_free_context(stream_context.format_context_);

			stream_context.format_context_ = nullptr;
			stream_context.codec_context_ = nullptr;
			stream_context.tmp_frame_ = nullptr;
			stream_context.sws_context_ = nullptr;
		}
		stream_contexts_.clear();
		if (hw_device_context_) 
		{
			av_buffer_unref(&hw_device_context_);
			hw_device_context_ = nullptr;
		}
	}

	auto Transcoder::setup_muxer(StreamContext& stream_context) -> std::tuple<bool, std::optional<std::string>> 
	{
		AVFormatContext* output_format_context = nullptr;
		if (avformat_alloc_output_context2(&output_format_context, nullptr, 
				stream_context.config.format.c_str(), stream_context.config.url.c_str()) < 0) 
		{
			return { false, "Failed to allocate output context" };
		}

		stream_context.format_context_ = output_format_context;

		AVStream* stream = avformat_new_stream(output_format_context, nullptr);
		stream->time_base = stream_context.codec_context_->time_base;
		avcodec_parameters_from_context(stream->codecpar, stream_context.codec_context_);
		stream_context.stream_ = stream;

		if (!(output_format_context->oformat->flags & AVFMT_NOFILE)) 
		{
			if (avio_open(&output_format_context->pb, stream_context.config.url.c_str(), AVIO_FLAG_WRITE) < 0) 
			{
				return { false, fmt::format("Failed to open output file: {}", stream_context.config.url) };
			}
		}


		if (stream_context.config.format == "hls")
		{
			av_opt_set(output_format_context, "hls_time", "4", 0);
			av_opt_set(output_format_context, "hls_list_size", "0", 0);
			av_opt_set(output_format_context, "hls_flags", "delete_segments+program_date_time", 0);
		}

		if (avformat_write_header(output_format_context, nullptr) < 0) 
		{
			return { false, fmt::format("Failed to write header {}", stream_context.config.url) };
		}

		return { true, std::nullopt };
	}

	auto Transcoder::scale_frame_cpu(const std::uint8_t* gpu_y, const std::uint8_t* gpu_uv, int stride_y, int stride_uv, StreamContext& stream_context)
		-> std::tuple<bool, std::optional<std::string>>
	{
		//placeholder code

		std::size_t frame_size_y = stream_context.config.height * stride_y;
		std::size_t frame_size_uv = stream_context.config.height / 2 * stride_uv;

#if 1
		cudaMemcpy(stream_context.tmp_frame_->data[0], gpu_y, frame_size_y, cudaMemcpyDeviceToHost);
		cudaMemcpy(stream_context.tmp_frame_->data[1], gpu_uv, frame_size_uv, cudaMemcpyDeviceToHost);
#else
		cudaMemcpy2DAsync(stream_context.tmp_frame_->data[0], stream_context.tmp_frame_->linesize[0],
			gpu_y, stride_y, stream_context.config.width, stream_context.config.height, cudaMemcpyDeviceToHost);
		cudaMemcpy2DAsync(stream_context.tmp_frame_->data[1], stream_context.tmp_frame_->linesize[1],
			gpu_uv, stride_uv, stream_context.config.width / 2, stream_context.config.height / 2, cudaMemcpyDeviceToHost);
#endif

		// TODO
		// validate this source code
		stream_context.tmp_frame_->linesize[0] = stream_context.config.width;
		stream_context.tmp_frame_->linesize[1] = stream_context.config.width;
		stream_context.tmp_frame_->pts = pts_;

		return { true, std::nullopt };
	}

	auto Transcoder::send_frame(StreamContext& stream_context, AVFrame* frame) -> std::tuple<bool, std::optional<std::string>>
	{
		if (avcodec_send_frame(stream_context.codec_context_, frame) < 0)
		{
			return { false, "Failed to send frame to encoder" };
		}

		AVPacket packet;
		av_init_packet(&packet);

		while (avcodec_receive_packet(stream_context.codec_context_, &packet) == 0)
		{
			packet.stream_index = stream_context.stream_->index;
			packet.pts = av_rescale_q(packet.pts, stream_context.codec_context_->time_base, stream_context.stream_->time_base);
			packet.dts = av_rescale_q(packet.dts, stream_context.codec_context_->time_base, stream_context.stream_->time_base);
			packet.duration = av_rescale_q(packet.duration, stream_context.codec_context_->time_base, stream_context.stream_->time_base);

			av_interleaved_write_frame(stream_context.format_context_, &packet);
			av_packet_unref(&packet);
		}
		return { true, std::nullopt };
	}

	auto Transcoder::setup_encoder(StreamContext& stream_context, int fps) -> std::tuple<bool, std::optional<std::string>> 
	{
		const AVCodec* codec = avcodec_find_encoder_by_name("h264_nvenc");
		if (!codec)
		{
			return { false, "Failed to find encoder" };
		}

		AVCodecContext* codec_context = avcodec_alloc_context3(codec);
		codec_context->width = stream_context.config.width;
		codec_context->height = stream_context.config.height;
		codec_context->time_base = AVRational{ 1, fps };
		codec_context->framerate = AVRational{ fps, 1 };
		codec_context->gop_size = fps * 2;
		codec_context->max_b_frames = 0;
		codec_context->pix_fmt = AV_PIX_FMT_CUDA;
		codec_context->bit_rate = stream_context.config.bit_rate;
		codec_context->hw_device_ctx = av_buffer_ref(hw_device_context_);

		// NVENC Options
		av_opt_set(codec_context->priv_data, "preset", "p4", 0);
		av_opt_set(codec_context->priv_data, "rc", "cbr", 0);
		av_opt_set_int(codec_context->priv_data, "forced-idr", 1, 0);

		if (avcodec_open2(codec_context, codec, nullptr) < 0)
		{
			return { false, "Could not open NVENC encoder" };
		}
		stream_context.codec_context_ = codec_context;

		stream_context.sws_context_ = sws_getContext(
			codec_context->width, codec_context->height, AV_PIX_FMT_NV12,
			codec_context->width, codec_context->height, AV_PIX_FMT_NV12,
			SWS_BILINEAR, nullptr, nullptr, nullptr);

		stream_context.tmp_frame_ = av_frame_alloc();
		stream_context.tmp_frame_->format = AV_PIX_FMT_NV12;
		stream_context.tmp_frame_->width = codec_context->width;
		stream_context.tmp_frame_->height = codec_context->height;
		av_frame_get_buffer(stream_context.tmp_frame_, 32);

		return { true, std::nullopt };
	}


}