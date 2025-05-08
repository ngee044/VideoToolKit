#pragma once

#include <vector>
#include <tuple>
#include <string>
#include <optional>
#include <memory>

namespace VideoToolKit
{
	enum class NvEnPreset
	{
		LowLatencyDefault = 0,
		LowLatencyHQ = 1,
		LowLatencyHP = 2,
		HighQuality = 3,
		HighPerformance = 4,
	};

	struct TranscodeProfile
	{
		int width;
		int height;
		int bit_rate;
		NvEnPreset preset;
	};

	struct GpuFrame
	{
		int width;
		int height;
		int bit_rate;
		NvEnPreset preset;
		std::string data; // Placeholder for actual GPU frame data
	};

	struct EncodedPacket 
	{
		int profile_index;
		std::uint8_t* data;
		std::size_t size;
		bool key_frame;
		int64_t pts;
		int64_t dts;
	};


	class Transcoder
	{
	public:
		Transcoder() = default;
		~Transcoder() = default;

		auto add_profile(int width, int height, int bit_rate, NvEnPreset preset) -> void;
		auto initialize(int frame_rate, int frame_rate_denominator) -> std::tuple<bool, std::optional<std::string>>;
		auto encode(const GpuFrame& frame) -> std::vector<EncodedPacket>;
		
		auto flush() -> std::tuple<bool, std::optional<std::string>>;

	protected:
		struct EncoderInstance 
		{
			TranscodeProfile profile;
			void* encoderSession;
		};

		auto init_cuda() -> std::tuple<bool, std::optional<std::string>>;
		bool resize_nv_12gpu(const GpuFrame& src, GpuFrame& dst);
		auto create_encoder_session(const TranscodeProfile& profile) -> std::tuple<bool, std::optional<std::string>>;
		auto encode_frame(EncoderInstance& encoder_instance, const GpuFrame& frame, std::vector<EncodedPacket>& packet) -> std::tuple<bool, std::optional<std::string>>;

	private:
		std::vector<EncoderInstance> encoders_;
		bool cuda_initialized_;
		void* cuda_context_;

	};
}