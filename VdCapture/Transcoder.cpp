#include "Transcoder.h"

#include <nvEncodeAPI.h>
#include <cuda.h>
#include <cuda_runtime.h>
#include <npp.h>

#include <iostream>
#include <cassert>


namespace VideoToolKit
{

static GUID preset_to_guid(NvEnPreset preset)
{
	switch (preset)
	{
	case NvEnPreset::LowLatencyDefault:
		return NV_ENC_PRESET_P1_GUID;
	case NvEnPreset::LowLatencyHQ:
		return NV_ENC_PRESET_P2_GUID;
	case NvEnPreset::LowLatencyHP:
		return NV_ENC_PRESET_P3_GUID;
	case NvEnPreset::HighQuality:
		return NV_ENC_PRESET_P4_GUID;
	case NvEnPreset::HighPerformance:
		return NV_ENC_PRESET_P5_GUID;
	default:
		assert(false && "Unknown preset type");
		return NV_ENC_PRESET_P6_GUID;
	}
}

Transcoder::Transcoder() : 
	cuda_initialized_(false),
	cuda_context_(nullptr)
{
}

Transcoder::~Transcoder() 
{
	flush();
}

auto Transcoder::flush() -> std::tuple<bool, std::optional<std::string>> 
{
	if (!cuda_initialized_)
	{
		return { false, "CUDA not initialized" };
	}

	for (auto& encoder : encoders_)
	{
		if (encoder.encoderSession != nullptr)
		{
			// TODO
			// Flush encoder session
			
			encoder.encoderSession = nullptr;
		}
	}

	cuda_initialized_ = false;
	return { true, std::nullopt };
}

auto Transcoder::add_profile(int width, int height, int bit_rate, NvEnPreset preset) -> void 
{
	TranscodeProfile profile;
	profile.width = width;
	profile.height = height;
	profile.bit_rate = bit_rate;
	profile.preset = preset;

	encoders_.emplace_back(EncoderInstance{ profile, nullptr });
}

auto Transcoder::initialize(int frame_rate, int frame_rate_denominator) -> std::tuple<bool, std::optional<std::string>>
{
	if (!cuda_initialized_)
	{
		return { false, "CUDA not initialized" };
	}

	for (auto& encoder : encoders_)
	{
		auto [success, error] = create_encoder_session(encoder.profile);
		if (!success)
		{
			return { false, error };
		}
	}

	return { true, std::nullopt };
}

auto Transcoder::encode(const GpuFrame& frame) -> std::vector<EncodedPacket>
{ 
	std::vector<EncodedPacket> output_packets;
	if (!cuda_initialized_)
	{
		// TODO
		// write error message to log
	}

	for (auto i = 0; i < encoders_.size(); ++i)
	{
		auto& encoder_instance = encoders_[i];
		
		GpuFrame scaled_frame;
		if (encoder_instance.profile.width == frame.width && encoder_instance.profile.height == frame.height)
		{
			scaled_frame = frame;	
		}
		else
		{
			resize_nv_12gpu(frame, scaled_frame);
		}

		std::vector<EncodedPacket> local_packets;
		auto [success, error] = encode_frame(encoder_instance, scaled_frame, local_packets);
		if (!success)
		{
			// TODO
			// write error message to log
		}

		for (auto& packet : local_packets)
		{
			packet.profile_index = i;
			output_packets.push_back(packet);
		}
	}

	return output_packets;
}

auto Transcoder::init_cuda() -> std::tuple<bool, std::optional<std::string>> 
{
	CUresult result = cuInit(0);

	if (result != CUDA_SUCCESS) 
	{
		return { false, "Failed to initialize CUDA" };
	}

	CUdevice device;
	cuDeviceGet(&device, 0);
	CUcontext context;
	cuCtxCreate(&context, 0, device);
	cuda_context_ = context;
	cuda_initialized_ = true;

	return { true, std::nullopt };
}

bool Transcoder::resize_nv_12gpu(const GpuFrame& src, GpuFrame& dst) 
{ 
	return false; 
}

auto Transcoder::create_encoder_session(const TranscodeProfile& profile) -> std::tuple<bool, std::optional<std::string>>
{
	return std::tuple<bool, std::optional<std::string>>();
}

auto Transcoder::encode_frame(EncoderInstance& encoder_instance, const GpuFrame& frame, std::vector<EncodedPacket>& packet) -> std::tuple<bool, std::optional<std::string>>
{
	// 
	return std::tuple<bool, std::optional<std::string>>();
}

}